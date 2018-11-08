// NAPS2.WIA.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <queue>
#include <functional>

class CWiaTransferCallback : public IWiaTransferCallback
{
public: // Constructors, destructor
	CWiaTransferCallback(void __stdcall statusCallback(LONG, LONG, ULONG64, HRESULT, IStream*)) : m_cRef(1), m_statusCallback(statusCallback) {};
	~CWiaTransferCallback() {};

public: // IWiaTransferCallback
	HRESULT __stdcall TransferCallback(
		LONG                lFlags,
		WiaTransferParams   *pWiaTransferParams) override
	{
		HRESULT hr = S_OK;
		IStream *stream = NULL;

		switch (pWiaTransferParams->lMessage)
		{
		case WIA_TRANSFER_MSG_STATUS:
			break;
		case WIA_TRANSFER_MSG_END_OF_STREAM:
			//...
			stream = m_streams.front();
			m_streams.pop();
			break;
		case WIA_TRANSFER_MSG_END_OF_TRANSFER:
			break;
		default:
			break;
		}

		m_statusCallback(
			pWiaTransferParams->lMessage,
			pWiaTransferParams->lPercentComplete,
			pWiaTransferParams->ulTransferredBytes,
			pWiaTransferParams->hrErrorStatus,
			stream);

		return hr;
	}

	HRESULT __stdcall GetNextStream(
		LONG    lFlags,
		BSTR    bstrItemName,
		BSTR    bstrFullItemName,
		IStream **ppDestination)
	{

		HRESULT hr = S_OK;

		//  Return a new stream for this item's data.
		//
		*ppDestination = SHCreateMemStream(nullptr, 0);
		m_streams.push(*ppDestination);
		return hr;
	}

public: // IUnknown
	//... // Etc.

	HRESULT CALLBACK QueryInterface(REFIID riid, void **ppvObject) override
	{
		// Validate arguments
		if (NULL == ppvObject)
		{
			return E_INVALIDARG;
		}

		// Return the appropropriate interface
		if (IsEqualIID(riid, IID_IUnknown))
		{
			*ppvObject = static_cast<IUnknown*>(this);
		}
		else if (IsEqualIID(riid, IID_IWiaTransferCallback))
		{
			*ppvObject = static_cast<IWiaTransferCallback*>(this);
		}
		else
		{
			*ppvObject = NULL;
			return (E_NOINTERFACE);
		}

		// Increment the reference count before we return the interface
		reinterpret_cast<IUnknown*>(*ppvObject)->AddRef();
		return S_OK;
	}

	ULONG CALLBACK AddRef() override
	{
		return InterlockedIncrement((long*)&m_cRef);
	}

	ULONG CALLBACK Release() override
	{
		LONG cRef = InterlockedDecrement((long*)&m_cRef);
		if (0 == cRef)
		{
			delete this;
		}
		return cRef;
	}

private:
	LONG m_cRef;
	std::queue<IStream*> m_streams;
	std::function<void(LONG, LONG, ULONG64, HRESULT, IStream*)> m_statusCallback;
};

extern "C" {

	__declspec(dllexport) HRESULT __stdcall GetDeviceManager(IWiaDevMgr2 **devMgr)
	{
		return CoCreateInstance(CLSID_WiaDevMgr2, NULL, CLSCTX_LOCAL_SERVER, IID_IWiaDevMgr2, (void**)devMgr);
	}

	__declspec(dllexport) void __stdcall Release(IUnknown *obj)
	{
		obj->Release();
	}

	__declspec(dllexport) HRESULT __stdcall GetDevice(IWiaDevMgr2 *devMgr, BSTR deviceId, IWiaItem2 **device)
	{
		return devMgr->CreateDevice(0, deviceId, device);
	}

	__declspec(dllexport) HRESULT __stdcall SetItemProperty(IWiaItem2 *item, int propId, int value)
	{
		IWiaPropertyStorage *propStorage = NULL;
		HRESULT hr = item->QueryInterface(IID_IWiaPropertyStorage, (void**)&propStorage);
		if (SUCCEEDED(hr))
		{
			PROPSPEC PropSpec[1] = { 0 };
			PROPVARIANT PropVariant[1] = { 0 };
			PropSpec[0].ulKind = PRSPEC_PROPID;
			PropSpec[0].propid = propId;
			PropVariant[0].vt = VT_I4;
			PropVariant[0].lVal = value;

			hr = propStorage->WriteMultiple(1, PropSpec, PropVariant, WIA_IPA_FIRST);
			propStorage->Release();
		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall GetItemPropertyBstr(IWiaItem2 *item, int propId, BSTR *value)
	{
		IWiaPropertyStorage *propStorage = NULL;
		HRESULT hr = item->QueryInterface(IID_IWiaPropertyStorage, (void**)&propStorage);
		if (SUCCEEDED(hr))
		{
			PROPSPEC PropSpec[1] = { 0 };
			PROPVARIANT PropVariant[1] = { 0 };
			PropSpec[0].ulKind = PRSPEC_PROPID;
			PropSpec[0].propid = propId;

			hr = propStorage->ReadMultiple(1, PropSpec, PropVariant);
			*value = PropVariant[0].bstrVal;
			propStorage->Release();
		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall GetItem(IWiaItem2 *device, BSTR itemId, IWiaItem2 **item)
	{
		//return device->FindItemByName(0, itemId, item);
		LONG itemType = 0;
		HRESULT hr = device->GetItemType(&itemType);
		if (SUCCEEDED(hr))
		{
			if (itemType & WiaItemTypeFolder || itemType & WiaItemTypeHasAttachments)
			{
				IEnumWiaItem2 *enumerator = NULL;
				hr = device->EnumChildItems(NULL, &enumerator);

				if (SUCCEEDED(hr))
				{
					int i = 0;
					while (S_OK == hr)
					{
						IWiaItem2 *childItem = NULL;
						hr = enumerator->Next(1, &childItem, NULL);

						if (S_OK == hr)
						{
							if (i++ == 1)
							{
								*item = childItem;
								break;
							}
							BSTR name;
							hr = GetItemPropertyBstr(childItem, 4099, &name);
							GetItem(childItem, itemId, item);
							/**item = childItem;
							*//*return hr;*/

							/*hr = GetItem(pChildWiaItem, itemId, item);
							pChildWiaItem->Release();
							pChildWiaItem = NULL;*/
						}
					}
					enumerator->Release();
					enumerator = NULL;
				}
			}
		}
		return hr;
	}

	__declspec(dllexport) HRESULT __stdcall StartTransfer(IWiaItem2 *item, IWiaTransfer **transfer)
	{
		return item->QueryInterface(IID_IWiaTransfer, (void**)transfer);
	}

	__declspec(dllexport) HRESULT __stdcall Download(IWiaTransfer *transfer, int flags, void __stdcall statusCallback(LONG, LONG, ULONG64, HRESULT, IStream*))
	{
		CWiaTransferCallback *callbackClass = new CWiaTransferCallback(statusCallback);
		if (callbackClass)
		{
			return transfer->Download(flags, callbackClass);
		}
		return S_FALSE;
	}

}