#include "DXFrameProcessor.h"
#include "DXCommon.h"
#include <string>

namespace SL {
	namespace Screen_Capture {
		class AquireFrameRAII {
			IDXGIOutputDuplication* _DuplLock;
			bool AquiredLock;
			void TryRelease() {
				if (AquiredLock) {
					auto hr = _DuplLock->ReleaseFrame();
					if (FAILED(hr) && hr != DXGI_ERROR_WAIT_TIMEOUT)
					{
						ProcessFailure(nullptr, L"Failed to release frame in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
					}
				}
				AquiredLock = false;
			}
		public:
			AquireFrameRAII(IDXGIOutputDuplication* dupl) : _DuplLock(dupl), AquiredLock(false){	}

			~AquireFrameRAII() {
				TryRelease();
			}
			HRESULT AcquireNextFrame(UINT TimeoutInMilliseconds, DXGI_OUTDUPL_FRAME_INFO *pFrameInfo, IDXGIResource **ppDesktopResource) {
				auto hr= _DuplLock->AcquireNextFrame(TimeoutInMilliseconds, pFrameInfo, ppDesktopResource);
				TryRelease();
				AquiredLock = SUCCEEDED(hr);
				return hr;
			}
		};
		class MAPPED_SUBRESOURCERAII {
			ID3D11DeviceContext* _Context;
			ID3D11Resource *_Resource;
			UINT _Subresource;
		public:
			MAPPED_SUBRESOURCERAII(ID3D11DeviceContext* context) : _Context(context), _Resource(nullptr), _Subresource(0) {	}

			~MAPPED_SUBRESOURCERAII() {
				_Context->Unmap(_Resource, _Subresource);
			}
			HRESULT Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource) {
				if (_Resource != nullptr) {
					_Context->Unmap(_Resource, _Subresource);
				}
				_Resource = pResource;
				_Subresource = Subresource;
				return _Context->Map(_Resource, _Subresource, MapType, MapFlags, pMappedResource);
			}
		};



		DXFrameProcessor::DXFrameProcessor()
		{
			ImageBufferSize = 0;
		}

		DXFrameProcessor::~DXFrameProcessor()
		{

		}
		DUPL_RETURN DXFrameProcessor::Init(std::shared_ptr<THREAD_DATA> data) {
			DX_RESOURCES res;
			auto ret = Initialize(res);
			if (ret != DUPL_RETURN_SUCCESS) {
				return ret;
			}
			DUPLE_RESOURCES dupl;
			ret = Initialize(dupl, res.Device.Get(), data->SelectedMonitor.Index);
			if (ret != DUPL_RETURN_SUCCESS) {
				return ret;
			}
			Device = res.Device;
			DeviceContext = res.DeviceContext;
			OutputDuplication = dupl.OutputDuplication;
			OutputDesc = dupl.OutputDesc;
			Output = dupl.Output;

			Data = data;
			ImageBufferSize = data->SelectedMonitor.Width* data->SelectedMonitor.Height*PixelStride;
			ImageBuffer = std::make_unique<char[]>(ImageBufferSize);
			return ret;
		}
		//
		// Process a given frame and its metadata
		//
		DUPL_RETURN DXFrameProcessor::ProcessFrame()
		{
			auto Ret = DUPL_RETURN_SUCCESS;

			Microsoft::WRL::ComPtr<IDXGIResource> DesktopResource;
			DXGI_OUTDUPL_FRAME_INFO FrameInfo;
			AquireFrameRAII frame(OutputDuplication.Get());

			// Get new frame
			auto hr = frame.AcquireNextFrame(500, &FrameInfo, DesktopResource.GetAddressOf());
			if (hr == DXGI_ERROR_WAIT_TIMEOUT)
			{
				return DUPL_RETURN_SUCCESS;
			}
			else if (FAILED(hr))
			{
				return ProcessFailure(Device.Get(), L"Failed to acquire next frame in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
			}
			Microsoft::WRL::ComPtr<ID3D11Texture2D> aquireddesktopimage;
			// QI for IDXGIResource
			hr = DesktopResource.Get()->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void **>(aquireddesktopimage.GetAddressOf()));
			if (FAILED(hr))
			{
				return ProcessFailure(nullptr, L"Failed to QI for ID3D11Texture2D from acquired IDXGIResource in DUPLICATIONMANAGER", L"Error", hr);
			}

			D3D11_TEXTURE2D_DESC ThisDesc;
			aquireddesktopimage->GetDesc(&ThisDesc);

			if (!StagingSurf)
			{
				D3D11_TEXTURE2D_DESC StagingDesc;
				StagingDesc = ThisDesc;
				StagingDesc.BindFlags = 0;
				StagingDesc.Usage = D3D11_USAGE_STAGING;
				StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				StagingDesc.MiscFlags = 0;
				hr = Device->CreateTexture2D(&StagingDesc, nullptr, StagingSurf.GetAddressOf());
				if (FAILED(hr))
				{
					return ProcessFailure(Device.Get(), L"Failed to create staging texture for move rects", L"Error", hr, SystemTransitionsExpectedErrors);
				}
			}

			auto movecount = 0;
			auto dirtycount = 0;
			RECT* dirtyrects = nullptr;
			// Get metadata
			if (FrameInfo.TotalMetadataBufferSize > 0)
			{
				MetaDataBuffer.reserve(FrameInfo.TotalMetadataBufferSize);
				UINT bufsize = FrameInfo.TotalMetadataBufferSize;

				// Get move rectangles
				hr = OutputDuplication->GetFrameMoveRects(bufsize, reinterpret_cast<DXGI_OUTDUPL_MOVE_RECT*>(MetaDataBuffer.data()), &bufsize);
				if (FAILED(hr))
				{
					return ProcessFailure(nullptr, L"Failed to get frame move rects in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
				}
				movecount = bufsize / sizeof(DXGI_OUTDUPL_MOVE_RECT);

				dirtyrects = reinterpret_cast<RECT*>(MetaDataBuffer.data() + bufsize);
				bufsize = FrameInfo.TotalMetadataBufferSize - bufsize;

				// Get dirty rectangles
				hr = OutputDuplication->GetFrameDirtyRects(bufsize, dirtyrects, &bufsize);
				if (FAILED(hr))
				{
					return ProcessFailure(nullptr, L"Failed to get frame dirty rects in DUPLICATIONMANAGER", L"Error", hr, FrameInfoExpectedErrors);
				}
				dirtycount = bufsize / sizeof(RECT);
				//convert rects to their correct coords
				for (auto i = 0; i < dirtycount; i++) {
					dirtyrects[i] = ConvertRect(dirtyrects[i], OutputDesc);
				}
			}
			DeviceContext->CopyResource(StagingSurf.Get(), aquireddesktopimage.Get());

			D3D11_MAPPED_SUBRESOURCE MappingDesc;
			MAPPED_SUBRESOURCERAII mappedresrouce(DeviceContext.Get());
			hr = mappedresrouce.Map(StagingSurf.Get(), 0, D3D11_MAP_READ, 0, &MappingDesc);
			// Get the data
			if (MappingDesc.pData == NULL) {
				return ProcessFailure(Device.Get(), L"DrawSurface_GetPixelColor: Could not read the pixel color because the mapped subresource returned NULL", L"Error", hr, SystemTransitionsExpectedErrors);
			}
			auto startsrc = (char*)MappingDesc.pData;
			auto startdst = ImageBuffer.get();
			auto rowstride = PixelStride*Data->SelectedMonitor.Width;
			if (rowstride == MappingDesc.RowPitch) {//no need for multiple calls, there is no padding here
				memcpy(startdst, startsrc, rowstride*Data->SelectedMonitor.Height);
			}
			else {
				for (auto i = 0; i < Data->SelectedMonitor.Height; i++) {
					memcpy(startdst + (i* rowstride), startsrc + (i* MappingDesc.RowPitch), rowstride);
				}
			}



			ImageRect ret;

			// Process dirties 
			if (dirtycount > 0 && dirtyrects != nullptr && Data->CaptureDifMonitor)
			{
				for (auto i = 0; i < dirtycount; i++)
				{
					ret.left = dirtyrects[i].left;
					ret.top = dirtyrects[i].top;
					ret.bottom = dirtyrects[i].bottom;
					ret.right = dirtyrects[i].right;
					Data->CaptureDifMonitor(ImageBuffer.get(), PixelStride, Data->SelectedMonitor, ret);
				}

			}
			if (Data->CaptureEntireMonitor) {
				ret.left = ret.top = 0;
				ret.bottom = Data->SelectedMonitor.Height;
				ret.right = Data->SelectedMonitor.Width;
				Data->CaptureEntireMonitor(ImageBuffer.get(), PixelStride, Data->SelectedMonitor);
			}
			return Ret;
		}

	}
}