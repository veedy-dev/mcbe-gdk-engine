/* WinRT Microsoft.Windows.Storage.Pickers implementation.
 *
 * Copyright 2026 BedrockOnLinux contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "private.h"

#define ASYNC_CLOSED ((AsyncStatus)4)

struct pick_file_result
{
    IPickFileResult IPickFileResult_iface;
    LONG ref;
    HSTRING path;
};

struct file_open_picker
{
    IFileOpenPicker IFileOpenPicker_iface;
    LONG ref;
    HWND hwnd;
    PickerViewMode view_mode;
    PickerLocationId location;
    HSTRING commit_button_text;
    IVector_HSTRING *filters;
};

struct file_type_choice
{
    HSTRING name;
    IVector_HSTRING *extensions;
};

struct file_type_choices
{
    IMap_HSTRING_IInspectable IMap_HSTRING_IInspectable_iface;
    LONG ref;
    UINT32 size;
    UINT32 capacity;
    struct file_type_choice *entries;
};

struct file_save_picker
{
    IFileSavePicker IFileSavePicker_iface;
    LONG ref;
    HWND hwnd;
    PickerLocationId location;
    HSTRING commit_button_text;
    struct file_type_choices *choices;
    HSTRING default_extension;
    HSTRING suggested_file_name;
    HSTRING suggested_folder;
};

struct file_picker_operation
{
    IAsyncOperation_PickFileResult IAsyncOperation_PickFileResult_iface;
    IAsyncInfo IAsyncInfo_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    BOOL handler_set;
    BOOL cancel_requested;
    AsyncStatus status;
    HRESULT error;
    IAsyncOperationCompletedHandler_PickFileResult *handler;
    IPickFileResult *result;

    HWND hwnd;
    PickerLocationId location;
    BOOL save;
    HSTRING commit_button_text;
    HSTRING default_extension;
    HSTRING suggested_file_name;
    HSTRING suggested_folder;
    UINT32 filter_count;
    HSTRING *filters;
};

struct pick_file_result_vector
{
    IVectorView_PickFileResult IVectorView_PickFileResult_iface;
    LONG ref;
    UINT32 size;
    IPickFileResult **items;
};

struct multi_file_picker_operation
{
    IAsyncOperation_IVectorView_PickFileResult IAsyncOperation_IVectorView_PickFileResult_iface;
    IAsyncInfo IAsyncInfo_iface;
    LONG ref;
    CRITICAL_SECTION cs;
    HANDLE thread;
    BOOL handler_set;
    BOOL cancel_requested;
    AsyncStatus status;
    HRESULT error;
    IAsyncOperationCompletedHandler_IVectorView_PickFileResult *handler;
    IVectorView_PickFileResult *result;

    HWND hwnd;
    PickerLocationId location;
    HSTRING commit_button_text;
    UINT32 filter_count;
    HSTRING *filters;
};

struct file_open_picker_factory
{
    IActivationFactory IActivationFactory_iface;
    IFileOpenPickerFactory IFileOpenPickerFactory_iface;
    LONG ref;
};

struct file_save_picker_factory
{
    IActivationFactory IActivationFactory_iface;
    IFileSavePickerFactory IFileSavePickerFactory_iface;
    LONG ref;
};

static const IID IID_IFileTypeChoices =
    {0xe475ca9d, 0x6afb, 0x5992, {0x99, 0x3e, 0x53, 0xe6, 0xef, 0x7a, 0x9e, 0xcd}};

static inline struct pick_file_result *result_from_iface( IPickFileResult *iface )
{
    return CONTAINING_RECORD( iface, struct pick_file_result, IPickFileResult_iface );
}

static inline struct file_open_picker *picker_from_iface( IFileOpenPicker *iface )
{
    return CONTAINING_RECORD( iface, struct file_open_picker, IFileOpenPicker_iface );
}

static inline struct file_type_choices *choices_from_iface( IMap_HSTRING_IInspectable *iface )
{
    return CONTAINING_RECORD( iface, struct file_type_choices, IMap_HSTRING_IInspectable_iface );
}

static inline struct file_save_picker *save_picker_from_iface( IFileSavePicker *iface )
{
    return CONTAINING_RECORD( iface, struct file_save_picker, IFileSavePicker_iface );
}

static inline struct file_picker_operation *operation_from_iface( IAsyncOperation_PickFileResult *iface )
{
    return CONTAINING_RECORD( iface, struct file_picker_operation, IAsyncOperation_PickFileResult_iface );
}

static inline struct file_picker_operation *operation_from_info( IAsyncInfo *iface )
{
    return CONTAINING_RECORD( iface, struct file_picker_operation, IAsyncInfo_iface );
}

static inline struct pick_file_result_vector *result_vector_from_iface( IVectorView_PickFileResult *iface )
{
    return CONTAINING_RECORD( iface, struct pick_file_result_vector, IVectorView_PickFileResult_iface );
}

static inline struct multi_file_picker_operation *multi_operation_from_iface(
    IAsyncOperation_IVectorView_PickFileResult *iface )
{
    return CONTAINING_RECORD( iface, struct multi_file_picker_operation,
                              IAsyncOperation_IVectorView_PickFileResult_iface );
}

static inline struct multi_file_picker_operation *multi_operation_from_info( IAsyncInfo *iface )
{
    return CONTAINING_RECORD( iface, struct multi_file_picker_operation, IAsyncInfo_iface );
}

static inline struct file_open_picker_factory *factory_from_activation( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct file_open_picker_factory, IActivationFactory_iface );
}

static inline struct file_open_picker_factory *factory_from_picker_factory( IFileOpenPickerFactory *iface )
{
    return CONTAINING_RECORD( iface, struct file_open_picker_factory, IFileOpenPickerFactory_iface );
}

static inline struct file_save_picker_factory *save_factory_from_activation( IActivationFactory *iface )
{
    return CONTAINING_RECORD( iface, struct file_save_picker_factory, IActivationFactory_iface );
}

static inline struct file_save_picker_factory *save_factory_from_picker_factory( IFileSavePickerFactory *iface )
{
    return CONTAINING_RECORD( iface, struct file_save_picker_factory, IFileSavePickerFactory_iface );
}

static HRESULT get_iids( ULONG *iid_count, IID **iids, UINT count, const IID *const *values )
{
    UINT i;

    if (!iid_count || !iids) return E_POINTER;
    *iid_count = 0;
    *iids = NULL;
    if (!(*iids = CoTaskMemAlloc( count * sizeof(**iids) ))) return E_OUTOFMEMORY;
    for (i = 0; i < count; ++i) (*iids)[i] = *values[i];
    *iid_count = count;
    return S_OK;
}

static HRESULT get_runtime_class_name( const WCHAR *name, HSTRING *class_name )
{
    if (!class_name) return E_POINTER;
    return WindowsCreateString( name, wcslen( name ), class_name );
}

static HRESULT get_trust_level( TrustLevel *trust_level )
{
    if (!trust_level) return E_POINTER;
    *trust_level = BaseTrust;
    return S_OK;
}

static HRESULT WINAPI choices_QueryInterface( IMap_HSTRING_IInspectable *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_IFileTypeChoices ))
        return E_NOINTERFACE;
    IMap_HSTRING_IInspectable_AddRef( iface );
    *out = iface;
    return S_OK;
}

static ULONG WINAPI choices_AddRef( IMap_HSTRING_IInspectable *iface )
{
    return InterlockedIncrement( &choices_from_iface( iface )->ref );
}

static HRESULT WINAPI choices_Clear( IMap_HSTRING_IInspectable *iface );

static ULONG WINAPI choices_Release( IMap_HSTRING_IInspectable *iface )
{
    struct file_type_choices *choices = choices_from_iface( iface );
    ULONG ref = InterlockedDecrement( &choices->ref );

    if (!ref)
    {
        choices_Clear( iface );
        free( choices );
    }
    return ref;
}

static HRESULT WINAPI choices_GetIids( IMap_HSTRING_IInspectable *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IFileTypeChoices};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI choices_GetRuntimeClassName( IMap_HSTRING_IInspectable *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI choices_GetTrustLevel( IMap_HSTRING_IInspectable *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static struct file_type_choice *choices_find( struct file_type_choices *choices, HSTRING key )
{
    UINT32 i;
    int order;

    for (i = 0; i < choices->size; ++i)
        if (SUCCEEDED(WindowsCompareStringOrdinal( choices->entries[i].name, key, &order )) && !order)
            return &choices->entries[i];
    return NULL;
}

static HRESULT WINAPI choices_Lookup( IMap_HSTRING_IInspectable *iface, HSTRING key,
                                      IInspectable **value )
{
    struct file_type_choice *entry;

    if (!value) return E_POINTER;
    *value = NULL;
    if (!(entry = choices_find( choices_from_iface( iface ), key ))) return E_BOUNDS;
    IVector_HSTRING_AddRef( entry->extensions );
    *value = (IInspectable *)entry->extensions;
    return S_OK;
}

static HRESULT WINAPI choices_get_Size( IMap_HSTRING_IInspectable *iface, UINT32 *value )
{
    if (!value) return E_POINTER;
    *value = choices_from_iface( iface )->size;
    return S_OK;
}

static HRESULT WINAPI choices_HasKey( IMap_HSTRING_IInspectable *iface, HSTRING key,
                                      boolean *found )
{
    if (!found) return E_POINTER;
    *found = choices_find( choices_from_iface( iface ), key ) != NULL;
    return S_OK;
}

static HRESULT WINAPI choices_GetView( IMap_HSTRING_IInspectable *iface,
                                       IMapView_HSTRING_IInspectable **value )
{
    if (!value) return E_POINTER;
    *value = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI choices_Insert( IMap_HSTRING_IInspectable *iface, HSTRING key,
                                      IInspectable *value, boolean *replaced )
{
    struct file_type_choices *choices = choices_from_iface( iface );
    struct file_type_choice *entry;
    IVector_HSTRING *extensions;
    HSTRING name;
    HRESULT hr;

    if (!value || !replaced) return E_POINTER;
    if (FAILED(hr = IInspectable_QueryInterface( value, &IID_IVector_HSTRING,
                                                 (void **)&extensions ))) return hr;
    if ((entry = choices_find( choices, key )))
    {
        IVector_HSTRING_Release( entry->extensions );
        entry->extensions = extensions;
        *replaced = TRUE;
        return S_OK;
    }
    if (FAILED(hr = WindowsDuplicateString( key, &name )))
    {
        IVector_HSTRING_Release( extensions );
        return hr;
    }
    if (choices->size == choices->capacity)
    {
        UINT32 capacity = max( 4, choices->capacity * 2 );
        void *entries = realloc( choices->entries, capacity * sizeof(*choices->entries) );
        if (!entries)
        {
            WindowsDeleteString( name );
            IVector_HSTRING_Release( extensions );
            return E_OUTOFMEMORY;
        }
        choices->entries = entries;
        choices->capacity = capacity;
    }
    entry = &choices->entries[choices->size++];
    entry->name = name;
    entry->extensions = extensions;
    *replaced = FALSE;
    return S_OK;
}

static HRESULT WINAPI choices_Remove( IMap_HSTRING_IInspectable *iface, HSTRING key )
{
    struct file_type_choices *choices = choices_from_iface( iface );
    struct file_type_choice *entry;

    if (!(entry = choices_find( choices, key ))) return E_BOUNDS;
    WindowsDeleteString( entry->name );
    IVector_HSTRING_Release( entry->extensions );
    memmove( entry, entry + 1,
             (choices->entries + --choices->size - entry) * sizeof(*choices->entries) );
    return S_OK;
}

static HRESULT WINAPI choices_Clear( IMap_HSTRING_IInspectable *iface )
{
    struct file_type_choices *choices = choices_from_iface( iface );

    while (choices->size)
    {
        struct file_type_choice *entry = &choices->entries[--choices->size];
        WindowsDeleteString( entry->name );
        IVector_HSTRING_Release( entry->extensions );
    }
    free( choices->entries );
    choices->entries = NULL;
    choices->capacity = 0;
    return S_OK;
}

static const IMap_HSTRING_IInspectableVtbl choices_vtbl =
{
    choices_QueryInterface,
    choices_AddRef,
    choices_Release,
    choices_GetIids,
    choices_GetRuntimeClassName,
    choices_GetTrustLevel,
    choices_Lookup,
    choices_get_Size,
    choices_HasKey,
    choices_GetView,
    choices_Insert,
    choices_Remove,
    choices_Clear,
};

static HRESULT file_type_choices_create( struct file_type_choices **out )
{
    struct file_type_choices *choices;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(choices = calloc( 1, sizeof(*choices) ))) return E_OUTOFMEMORY;
    choices->IMap_HSTRING_IInspectable_iface.lpVtbl = &choices_vtbl;
    choices->ref = 1;
    *out = choices;
    return S_OK;
}

static HRESULT WINAPI result_QueryInterface( IPickFileResult *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_IPickFileResult ))
        return E_NOINTERFACE;
    IPickFileResult_AddRef( iface );
    *out = iface;
    return S_OK;
}

static ULONG WINAPI result_AddRef( IPickFileResult *iface )
{
    return InterlockedIncrement( &result_from_iface( iface )->ref );
}

static ULONG WINAPI result_Release( IPickFileResult *iface )
{
    struct pick_file_result *result = result_from_iface( iface );
    ULONG ref = InterlockedDecrement( &result->ref );
    if (!ref)
    {
        WindowsDeleteString( result->path );
        free( result );
    }
    return ref;
}

static HRESULT WINAPI result_GetIids( IPickFileResult *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IPickFileResult};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI result_GetRuntimeClassName( IPickFileResult *iface, HSTRING *name )
{
    return get_runtime_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_PickFileResult, name );
}

static HRESULT WINAPI result_GetTrustLevel( IPickFileResult *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI result_get_Path( IPickFileResult *iface, HSTRING *value )
{
    struct pick_file_result *result = result_from_iface( iface );
    if (!value) return E_POINTER;
    return WindowsDuplicateString( result->path, value );
}

static const IPickFileResultVtbl result_vtbl =
{
    result_QueryInterface,
    result_AddRef,
    result_Release,
    result_GetIids,
    result_GetRuntimeClassName,
    result_GetTrustLevel,
    result_get_Path,
};

static HRESULT pick_file_result_create( const WCHAR *path, IPickFileResult **out )
{
    struct pick_file_result *result;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(result = calloc( 1, sizeof(*result) ))) return E_OUTOFMEMORY;
    result->IPickFileResult_iface.lpVtbl = &result_vtbl;
    result->ref = 1;
    if (FAILED(hr = WindowsCreateString( path, wcslen( path ), &result->path )))
    {
        free( result );
        return hr;
    }
    *out = &result->IPickFileResult_iface;
    return S_OK;
}

static HRESULT WINAPI result_vector_QueryInterface( IVectorView_PickFileResult *iface,
                                                     REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) &&
        !IsEqualGUID( iid, &IID_IVectorView_PickFileResult ))
        return E_NOINTERFACE;
    IVectorView_PickFileResult_AddRef( iface );
    *out = iface;
    return S_OK;
}

static ULONG WINAPI result_vector_AddRef( IVectorView_PickFileResult *iface )
{
    return InterlockedIncrement( &result_vector_from_iface( iface )->ref );
}

static ULONG WINAPI result_vector_Release( IVectorView_PickFileResult *iface )
{
    struct pick_file_result_vector *vector = result_vector_from_iface( iface );
    ULONG ref = InterlockedDecrement( &vector->ref );
    UINT32 i;

    if (!ref)
    {
        for (i = 0; i < vector->size; ++i) IPickFileResult_Release( vector->items[i] );
        free( vector->items );
        free( vector );
    }
    return ref;
}

static HRESULT WINAPI result_vector_GetIids( IVectorView_PickFileResult *iface,
                                              ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IVectorView_PickFileResult};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI result_vector_GetRuntimeClassName( IVectorView_PickFileResult *iface,
                                                          HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI result_vector_GetTrustLevel( IVectorView_PickFileResult *iface,
                                                    TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI result_vector_GetAt( IVectorView_PickFileResult *iface, UINT32 index,
                                           IPickFileResult **value )
{
    struct pick_file_result_vector *vector = result_vector_from_iface( iface );

    if (!value) return E_POINTER;
    *value = NULL;
    if (index >= vector->size) return E_BOUNDS;
    IPickFileResult_AddRef( *value = vector->items[index] );
    return S_OK;
}

static HRESULT WINAPI result_vector_get_Size( IVectorView_PickFileResult *iface, UINT32 *value )
{
    if (!value) return E_POINTER;
    *value = result_vector_from_iface( iface )->size;
    return S_OK;
}

static HRESULT WINAPI result_vector_IndexOf( IVectorView_PickFileResult *iface,
                                              IPickFileResult *element, UINT32 *index,
                                              boolean *found )
{
    struct pick_file_result_vector *vector = result_vector_from_iface( iface );
    UINT32 i;

    if (!index || !found) return E_POINTER;
    for (i = 0; i < vector->size; ++i) if (vector->items[i] == element) break;
    *found = i < vector->size;
    *index = *found ? i : 0;
    return S_OK;
}

static HRESULT WINAPI result_vector_GetMany( IVectorView_PickFileResult *iface,
                                              UINT32 start, UINT32 capacity,
                                              IPickFileResult **items, UINT32 *count )
{
    struct pick_file_result_vector *vector = result_vector_from_iface( iface );
    UINT32 i, available;

    if (!count || (capacity && !items)) return E_POINTER;
    *count = 0;
    if (start >= vector->size) return S_OK;
    available = min( capacity, vector->size - start );
    for (i = 0; i < available; ++i)
        IPickFileResult_AddRef( items[i] = vector->items[start + i] );
    *count = available;
    return S_OK;
}

static const IVectorView_PickFileResultVtbl result_vector_vtbl =
{
    result_vector_QueryInterface,
    result_vector_AddRef,
    result_vector_Release,
    result_vector_GetIids,
    result_vector_GetRuntimeClassName,
    result_vector_GetTrustLevel,
    result_vector_GetAt,
    result_vector_get_Size,
    result_vector_IndexOf,
    result_vector_GetMany,
};

/* Takes ownership of items and each contained reference on success. */
static HRESULT pick_file_result_vector_create( IPickFileResult **items, UINT32 count,
                                               IVectorView_PickFileResult **out )
{
    struct pick_file_result_vector *vector;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(vector = calloc( 1, sizeof(*vector) ))) return E_OUTOFMEMORY;
    vector->IVectorView_PickFileResult_iface.lpVtbl = &result_vector_vtbl;
    vector->ref = 1;
    vector->size = count;
    vector->items = items;
    *out = &vector->IVectorView_PickFileResult_iface;
    return S_OK;
}

static HRESULT operation_query_interface( struct file_picker_operation *operation, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IAsyncOperation_PickFileResult ))
        *out = &operation->IAsyncOperation_PickFileResult_iface;
    else if (IsEqualGUID( iid, &IID_IAsyncInfo ))
        *out = &operation->IAsyncInfo_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &operation->ref );
    return S_OK;
}

static ULONG operation_add_ref( struct file_picker_operation *operation )
{
    return InterlockedIncrement( &operation->ref );
}

static ULONG operation_release( struct file_picker_operation *operation )
{
    ULONG ref = InterlockedDecrement( &operation->ref );
    UINT32 i;

    if (!ref)
    {
        if (operation->handler) IAsyncOperationCompletedHandler_PickFileResult_Release( operation->handler );
        if (operation->result) IPickFileResult_Release( operation->result );
        WindowsDeleteString( operation->commit_button_text );
        WindowsDeleteString( operation->default_extension );
        WindowsDeleteString( operation->suggested_file_name );
        WindowsDeleteString( operation->suggested_folder );
        for (i = 0; i < operation->filter_count; ++i) WindowsDeleteString( operation->filters[i] );
        free( operation->filters );
        operation->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &operation->cs );
        free( operation );
    }
    return ref;
}

static HRESULT WINAPI operation_QueryInterface( IAsyncOperation_PickFileResult *iface, REFIID iid, void **out )
{
    return operation_query_interface( operation_from_iface( iface ), iid, out );
}

static ULONG WINAPI operation_AddRef( IAsyncOperation_PickFileResult *iface )
{
    return operation_add_ref( operation_from_iface( iface ) );
}

static ULONG WINAPI operation_Release( IAsyncOperation_PickFileResult *iface )
{
    return operation_release( operation_from_iface( iface ) );
}

static HRESULT WINAPI operation_GetIids( IAsyncOperation_PickFileResult *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IAsyncOperation_PickFileResult, &IID_IAsyncInfo};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI operation_GetRuntimeClassName( IAsyncOperation_PickFileResult *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI operation_GetTrustLevel( IAsyncOperation_PickFileResult *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI operation_put_Completed( IAsyncOperation_PickFileResult *iface,
                                               IAsyncOperationCompletedHandler_PickFileResult *handler )
{
    struct file_picker_operation *operation = operation_from_iface( iface );
    IAsyncOperationCompletedHandler_PickFileResult *invoke = NULL;
    AsyncStatus status = Started;
    HRESULT hr = S_OK;

    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else if (operation->handler_set)
        hr = E_ILLEGAL_DELEGATE_ASSIGNMENT;
    else
    {
        operation->handler_set = TRUE;
        if (handler && operation->status != Started)
        {
            IAsyncOperationCompletedHandler_PickFileResult_AddRef( invoke = handler );
            status = operation->status;
            operation_add_ref( operation );
        }
        else if ((operation->handler = handler))
            IAsyncOperationCompletedHandler_PickFileResult_AddRef( handler );
    }
    LeaveCriticalSection( &operation->cs );

    if (invoke)
    {
        IAsyncOperationCompletedHandler_PickFileResult_Invoke( invoke, iface, status );
        IAsyncOperationCompletedHandler_PickFileResult_Release( invoke );
        operation_release( operation );
    }
    return hr;
}

static HRESULT WINAPI operation_get_Completed( IAsyncOperation_PickFileResult *iface,
                                               IAsyncOperationCompletedHandler_PickFileResult **handler )
{
    struct file_picker_operation *operation = operation_from_iface( iface );
    HRESULT hr = S_OK;

    if (!handler) return E_POINTER;
    *handler = NULL;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else if (operation->handler)
        IAsyncOperationCompletedHandler_PickFileResult_AddRef( *handler = operation->handler );
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI operation_GetResults( IAsyncOperation_PickFileResult *iface, IPickFileResult **results )
{
    struct file_picker_operation *operation = operation_from_iface( iface );
    HRESULT hr;

    if (!results) return E_POINTER;
    *results = NULL;
    EnterCriticalSection( &operation->cs );
    if (operation->status == Completed)
    {
        if (operation->result) IPickFileResult_AddRef( *results = operation->result );
        hr = S_OK;
    }
    else if (operation->status == Error)
        hr = operation->error;
    else if (operation->status == Canceled)
        hr = E_ABORT;
    else
        hr = E_ILLEGAL_METHOD_CALL;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static const IAsyncOperation_PickFileResultVtbl operation_vtbl =
{
    operation_QueryInterface,
    operation_AddRef,
    operation_Release,
    operation_GetIids,
    operation_GetRuntimeClassName,
    operation_GetTrustLevel,
    operation_put_Completed,
    operation_get_Completed,
    operation_GetResults,
};

static HRESULT WINAPI async_info_QueryInterface( IAsyncInfo *iface, REFIID iid, void **out )
{
    return operation_query_interface( operation_from_info( iface ), iid, out );
}

static ULONG WINAPI async_info_AddRef( IAsyncInfo *iface )
{
    return operation_add_ref( operation_from_info( iface ) );
}

static ULONG WINAPI async_info_Release( IAsyncInfo *iface )
{
    return operation_release( operation_from_info( iface ) );
}

static HRESULT WINAPI async_info_GetIids( IAsyncInfo *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IAsyncInfo, &IID_IAsyncOperation_PickFileResult};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI async_info_GetRuntimeClassName( IAsyncInfo *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI async_info_GetTrustLevel( IAsyncInfo *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI async_info_get_Id( IAsyncInfo *iface, UINT32 *id )
{
    struct file_picker_operation *operation = operation_from_info( iface );
    HRESULT hr = S_OK;
    if (!id) return E_POINTER;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED) hr = E_ILLEGAL_METHOD_CALL;
    else *id = 1;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI async_info_get_Status( IAsyncInfo *iface, AsyncStatus *status )
{
    struct file_picker_operation *operation = operation_from_info( iface );
    HRESULT hr = S_OK;
    if (!status) return E_POINTER;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED) hr = E_ILLEGAL_METHOD_CALL;
    else *status = operation->status;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI async_info_get_ErrorCode( IAsyncInfo *iface, HRESULT *error )
{
    struct file_picker_operation *operation = operation_from_info( iface );
    HRESULT hr = S_OK;
    if (!error) return E_POINTER;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else
        *error = operation->status == Error ? operation->error : S_OK;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI async_info_Cancel( IAsyncInfo *iface )
{
    struct file_picker_operation *operation = operation_from_info( iface );
    HRESULT hr = S_OK;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else if (operation->status == Started)
        operation->cancel_requested = TRUE;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI async_info_Close( IAsyncInfo *iface )
{
    struct file_picker_operation *operation = operation_from_info( iface );
    HRESULT hr = S_OK;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else if (operation->status == Started)
        hr = E_ILLEGAL_STATE_CHANGE;
    else
        operation->status = ASYNC_CLOSED;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static const IAsyncInfoVtbl async_info_vtbl =
{
    async_info_QueryInterface,
    async_info_AddRef,
    async_info_Release,
    async_info_GetIids,
    async_info_GetRuntimeClassName,
    async_info_GetTrustLevel,
    async_info_get_Id,
    async_info_get_Status,
    async_info_get_ErrorCode,
    async_info_Cancel,
    async_info_Close,
};

static HRESULT multi_operation_query_interface( struct multi_file_picker_operation *operation,
                                                REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) ||
        IsEqualGUID( iid, &IID_IAsyncOperation_IVectorView_PickFileResult ))
        *out = &operation->IAsyncOperation_IVectorView_PickFileResult_iface;
    else if (IsEqualGUID( iid, &IID_IAsyncInfo ))
        *out = &operation->IAsyncInfo_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &operation->ref );
    return S_OK;
}

static ULONG multi_operation_add_ref( struct multi_file_picker_operation *operation )
{
    return InterlockedIncrement( &operation->ref );
}

static ULONG multi_operation_release( struct multi_file_picker_operation *operation )
{
    ULONG ref = InterlockedDecrement( &operation->ref );
    UINT32 i;

    if (!ref)
    {
        if (operation->thread) CloseHandle( operation->thread );
        if (operation->handler)
            IAsyncOperationCompletedHandler_IVectorView_PickFileResult_Release( operation->handler );
        if (operation->result) IVectorView_PickFileResult_Release( operation->result );
        WindowsDeleteString( operation->commit_button_text );
        for (i = 0; i < operation->filter_count; ++i) WindowsDeleteString( operation->filters[i] );
        free( operation->filters );
        operation->cs.DebugInfo->Spare[0] = 0;
        DeleteCriticalSection( &operation->cs );
        free( operation );
    }
    return ref;
}

static HRESULT WINAPI multi_operation_QueryInterface(
    IAsyncOperation_IVectorView_PickFileResult *iface, REFIID iid, void **out )
{
    return multi_operation_query_interface( multi_operation_from_iface( iface ), iid, out );
}

static ULONG WINAPI multi_operation_AddRef( IAsyncOperation_IVectorView_PickFileResult *iface )
{
    return multi_operation_add_ref( multi_operation_from_iface( iface ) );
}

static ULONG WINAPI multi_operation_Release( IAsyncOperation_IVectorView_PickFileResult *iface )
{
    return multi_operation_release( multi_operation_from_iface( iface ) );
}

static HRESULT WINAPI multi_operation_GetIids( IAsyncOperation_IVectorView_PickFileResult *iface,
                                                ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IAsyncOperation_IVectorView_PickFileResult, &IID_IAsyncInfo};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI multi_operation_GetRuntimeClassName(
    IAsyncOperation_IVectorView_PickFileResult *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI multi_operation_GetTrustLevel(
    IAsyncOperation_IVectorView_PickFileResult *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI multi_operation_put_Completed(
    IAsyncOperation_IVectorView_PickFileResult *iface,
    IAsyncOperationCompletedHandler_IVectorView_PickFileResult *handler )
{
    struct multi_file_picker_operation *operation = multi_operation_from_iface( iface );
    IAsyncOperationCompletedHandler_IVectorView_PickFileResult *invoke = NULL;
    AsyncStatus status = Started;
    HRESULT hr = S_OK;

    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else if (operation->handler_set)
        hr = E_ILLEGAL_DELEGATE_ASSIGNMENT;
    else
    {
        operation->handler_set = TRUE;
        if (handler && operation->status != Started)
        {
            IAsyncOperationCompletedHandler_IVectorView_PickFileResult_AddRef( invoke = handler );
            status = operation->status;
            multi_operation_add_ref( operation );
        }
        else if ((operation->handler = handler))
            IAsyncOperationCompletedHandler_IVectorView_PickFileResult_AddRef( handler );
    }
    LeaveCriticalSection( &operation->cs );

    if (invoke)
    {
        IAsyncOperationCompletedHandler_IVectorView_PickFileResult_Invoke( invoke, iface, status );
        IAsyncOperationCompletedHandler_IVectorView_PickFileResult_Release( invoke );
        multi_operation_release( operation );
    }
    return hr;
}

static HRESULT WINAPI multi_operation_get_Completed(
    IAsyncOperation_IVectorView_PickFileResult *iface,
    IAsyncOperationCompletedHandler_IVectorView_PickFileResult **handler )
{
    struct multi_file_picker_operation *operation = multi_operation_from_iface( iface );
    HRESULT hr = S_OK;

    if (!handler) return E_POINTER;
    *handler = NULL;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else if (operation->handler)
        IAsyncOperationCompletedHandler_IVectorView_PickFileResult_AddRef(
            *handler = operation->handler );
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI multi_operation_GetResults(
    IAsyncOperation_IVectorView_PickFileResult *iface, IVectorView_PickFileResult **results )
{
    struct multi_file_picker_operation *operation = multi_operation_from_iface( iface );
    HRESULT hr;

    if (!results) return E_POINTER;
    *results = NULL;
    EnterCriticalSection( &operation->cs );
    if (operation->status == Completed)
    {
        if (operation->result) IVectorView_PickFileResult_AddRef( *results = operation->result );
        hr = S_OK;
    }
    else if (operation->status == Error)
        hr = operation->error;
    else if (operation->status == Canceled)
        hr = E_ABORT;
    else
        hr = E_ILLEGAL_METHOD_CALL;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static const IAsyncOperation_IVectorView_PickFileResultVtbl multi_operation_vtbl =
{
    multi_operation_QueryInterface,
    multi_operation_AddRef,
    multi_operation_Release,
    multi_operation_GetIids,
    multi_operation_GetRuntimeClassName,
    multi_operation_GetTrustLevel,
    multi_operation_put_Completed,
    multi_operation_get_Completed,
    multi_operation_GetResults,
};

static HRESULT WINAPI multi_async_info_QueryInterface( IAsyncInfo *iface, REFIID iid, void **out )
{
    return multi_operation_query_interface( multi_operation_from_info( iface ), iid, out );
}

static ULONG WINAPI multi_async_info_AddRef( IAsyncInfo *iface )
{
    return multi_operation_add_ref( multi_operation_from_info( iface ) );
}

static ULONG WINAPI multi_async_info_Release( IAsyncInfo *iface )
{
    return multi_operation_release( multi_operation_from_info( iface ) );
}

static HRESULT WINAPI multi_async_info_GetIids( IAsyncInfo *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IAsyncInfo, &IID_IAsyncOperation_IVectorView_PickFileResult};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI multi_async_info_GetRuntimeClassName( IAsyncInfo *iface, HSTRING *name )
{
    if (!name) return E_POINTER;
    *name = NULL;
    return S_OK;
}

static HRESULT WINAPI multi_async_info_GetTrustLevel( IAsyncInfo *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI multi_async_info_get_Id( IAsyncInfo *iface, UINT32 *id )
{
    struct multi_file_picker_operation *operation = multi_operation_from_info( iface );
    HRESULT hr = S_OK;
    if (!id) return E_POINTER;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED) hr = E_ILLEGAL_METHOD_CALL;
    else *id = 1;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI multi_async_info_get_Status( IAsyncInfo *iface, AsyncStatus *status )
{
    struct multi_file_picker_operation *operation = multi_operation_from_info( iface );
    HRESULT hr = S_OK;
    if (!status) return E_POINTER;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED) hr = E_ILLEGAL_METHOD_CALL;
    else *status = operation->status;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI multi_async_info_get_ErrorCode( IAsyncInfo *iface, HRESULT *error )
{
    struct multi_file_picker_operation *operation = multi_operation_from_info( iface );
    HRESULT hr = S_OK;
    if (!error) return E_POINTER;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else
        *error = operation->status == Error ? operation->error : S_OK;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI multi_async_info_Cancel( IAsyncInfo *iface )
{
    struct multi_file_picker_operation *operation = multi_operation_from_info( iface );
    HRESULT hr = S_OK;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else if (operation->status == Started)
        operation->cancel_requested = TRUE;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static HRESULT WINAPI multi_async_info_Close( IAsyncInfo *iface )
{
    struct multi_file_picker_operation *operation = multi_operation_from_info( iface );
    HRESULT hr = S_OK;
    EnterCriticalSection( &operation->cs );
    if (operation->status == ASYNC_CLOSED)
        hr = E_ILLEGAL_METHOD_CALL;
    else if (operation->status == Started)
        hr = E_ILLEGAL_STATE_CHANGE;
    else
        operation->status = ASYNC_CLOSED;
    LeaveCriticalSection( &operation->cs );
    return hr;
}

static const IAsyncInfoVtbl multi_async_info_vtbl =
{
    multi_async_info_QueryInterface,
    multi_async_info_AddRef,
    multi_async_info_Release,
    multi_async_info_GetIids,
    multi_async_info_GetRuntimeClassName,
    multi_async_info_GetTrustLevel,
    multi_async_info_get_Id,
    multi_async_info_get_Status,
    multi_async_info_get_ErrorCode,
    multi_async_info_Cancel,
    multi_async_info_Close,
};

struct dialog_filters
{
    COMDLG_FILTERSPEC *specs;
    WCHAR **patterns;
    WCHAR *combined;
    UINT count;
    UINT pattern_count;
};

static void dialog_filters_destroy( struct dialog_filters *filters )
{
    UINT i;
    for (i = 0; i < filters->pattern_count; ++i) free( filters->patterns[i] );
    free( filters->patterns );
    free( filters->combined );
    free( filters->specs );
}

static HRESULT dialog_filters_create( UINT32 filter_count, HSTRING *filter_values,
                                      struct dialog_filters *out )
{
    static const WCHAR all_files[] = L"All Files";
    const WCHAR *value;
    UINT32 length, i;
    SIZE_T combined_length = 1;
    WCHAR *cursor;

    memset( out, 0, sizeof(*out) );
    out->count = filter_count ? filter_count + 1 : 1;
    if (!(out->specs = calloc( out->count, sizeof(*out->specs) ))) return E_OUTOFMEMORY;

    if (!filter_count)
    {
        out->specs[0].pszName = all_files;
        out->specs[0].pszSpec = L"*";
        return S_OK;
    }

    if (!(out->patterns = calloc( filter_count, sizeof(*out->patterns) )))
        goto oom;
    out->pattern_count = filter_count;
    for (i = 0; i < filter_count; ++i)
    {
        value = WindowsGetStringRawBuffer( filter_values[i], &length );
        if (!(out->patterns[i] = malloc( (length + 2) * sizeof(WCHAR) ))) goto oom;
        cursor = out->patterns[i];
        if (!length || value[0] != '*') *cursor++ = '*';
        memcpy( cursor, value, length * sizeof(WCHAR) );
        cursor[length] = 0;
        combined_length += wcslen( out->patterns[i] ) + (i != 0);
        out->specs[i].pszName = L"";
        out->specs[i].pszSpec = out->patterns[i];
    }

    if (filter_count == 1 && !wcscmp( out->patterns[0], L"*" ))
    {
        out->count = 1;
        out->specs[0].pszName = all_files;
        return S_OK;
    }

    if (!(out->combined = malloc( combined_length * sizeof(WCHAR) ))) goto oom;
    cursor = out->combined;
    for (i = 0; i < filter_count; ++i)
    {
        SIZE_T len = wcslen( out->patterns[i] );
        if (i) *cursor++ = ';';
        memcpy( cursor, out->patterns[i], len * sizeof(WCHAR) );
        cursor += len;
    }
    *cursor = 0;
    out->specs[filter_count].pszName = all_files;
    out->specs[filter_count].pszSpec = out->combined;
    return S_OK;

oom:
    dialog_filters_destroy( out );
    memset( out, 0, sizeof(*out) );
    return E_OUTOFMEMORY;
}

static const KNOWNFOLDERID *known_folder_from_location( PickerLocationId location )
{
    switch (location)
    {
    case PickerLocationId_DocumentsLibrary: return &FOLDERID_Documents;
    case PickerLocationId_ComputerFolder: return &FOLDERID_ComputerFolder;
    case PickerLocationId_Desktop: return &FOLDERID_Desktop;
    case PickerLocationId_Downloads: return &FOLDERID_Downloads;
    case PickerLocationId_MusicLibrary: return &FOLDERID_MusicLibrary;
    case PickerLocationId_PicturesLibrary: return &FOLDERID_PicturesLibrary;
    case PickerLocationId_VideosLibrary: return &FOLDERID_VideosLibrary;
    case PickerLocationId_Objects3D: return &FOLDERID_Objects3D;
    default: return NULL;
    }
}

static void set_default_folder( IFileDialog *dialog, PickerLocationId location )
{
    const KNOWNFOLDERID *folder_id = known_folder_from_location( location );
    IShellItem *item = NULL;
    WCHAR *path = NULL;

    if (!folder_id) return;
    if (SUCCEEDED(SHGetKnownFolderPath( folder_id, 0, NULL, &path )) &&
        SUCCEEDED(SHCreateItemFromParsingName( path, NULL, &IID_IShellItem, (void **)&item )))
    {
        IFileDialog_SetDefaultFolder( dialog, item );
        IShellItem_Release( item );
    }
    CoTaskMemFree( path );
}

static HRESULT show_file_dialog( struct file_picker_operation *operation, IPickFileResult **result )
{
    static const WCHAR supported_files[] = L"Supported files";
    struct dialog_filters filters;
    const KNOWNFOLDERID *folder_id;
    OPENFILENAMEW dialog = {0};
    const WCHAR *default_extension, *description, *initial_dir = NULL, *pattern, *suggested_name;
    WCHAR *filter = NULL, *cursor, *known_folder = NULL, *path = NULL;
    SIZE_T description_length, filter_length, pattern_length;
    UINT32 default_extension_length, suggested_name_length, suggested_folder_length;
    DWORD error;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (FAILED(hr = dialog_filters_create( operation->filter_count, operation->filters, &filters )))
        return hr;

    pattern = filters.combined ? filters.combined : filters.specs[0].pszSpec;
    description = !wcscmp( pattern, L"*" ) ? filters.specs[0].pszName : supported_files;
    description_length = wcslen( description );
    pattern_length = wcslen( pattern );
    filter_length = description_length + pattern_length + 3;
    if (!(filter = calloc( filter_length, sizeof(*filter) )) ||
        !(path = calloc( 32768, sizeof(*path) )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }
    cursor = filter;
    memcpy( cursor, description, description_length * sizeof(*cursor) );
    cursor += description_length + 1;
    memcpy( cursor, pattern, pattern_length * sizeof(*cursor) );

    initial_dir = WindowsGetStringRawBuffer( operation->suggested_folder, &suggested_folder_length );
    if (!suggested_folder_length)
    {
        folder_id = known_folder_from_location( operation->location );
        if (folder_id) SHGetKnownFolderPath( folder_id, 0, NULL, &known_folder );
        initial_dir = known_folder;
    }

    suggested_name = WindowsGetStringRawBuffer( operation->suggested_file_name,
                                                &suggested_name_length );
    if (suggested_name_length >= 32768)
    {
        hr = E_INVALIDARG;
        goto done;
    }
    memcpy( path, suggested_name, suggested_name_length * sizeof(*path) );

    default_extension = WindowsGetStringRawBuffer( operation->default_extension,
                                                   &default_extension_length );
    if (default_extension_length && default_extension[0] == '.')
        ++default_extension;

    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = operation->hwnd;
    dialog.lpstrFilter = filter;
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = path;
    dialog.nMaxFile = 32768;
    dialog.lpstrInitialDir = initial_dir;
    dialog.lpstrDefExt = default_extension;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                   (operation->save ? OFN_OVERWRITEPROMPT | OFN_NOREADONLYRETURN : OFN_FILEMUSTEXIST);

    if ((operation->save ? GetSaveFileNameW( &dialog ) : GetOpenFileNameW( &dialog )))
        hr = pick_file_result_create( path, result );
    else if ((error = CommDlgExtendedError()))
        hr = HRESULT_FROM_WIN32( error );
    else
        hr = S_OK;

done:
    CoTaskMemFree( known_folder );
    free( path );
    free( filter );
    dialog_filters_destroy( &filters );
    return hr;
}

static HRESULT show_multi_file_dialog( struct multi_file_picker_operation *operation,
                                       IVectorView_PickFileResult **result )
{
    struct dialog_filters filters;
    IFileOpenDialog *dialog = NULL;
    IFileDialog *file_dialog = NULL;
    IShellItemArray *item_array = NULL;
    IShellItem *item = NULL;
    IPickFileResult **items = NULL;
    FILEOPENDIALOGOPTIONS options;
    const WCHAR *commit;
    WCHAR *path = NULL;
    DWORD count = 0;
    UINT32 commit_length, created = 0, i;
    HRESULT hr;

    if (!result) return E_POINTER;
    *result = NULL;
    if (FAILED(hr = dialog_filters_create( operation->filter_count, operation->filters, &filters )))
        return hr;
    if (FAILED(hr = CoCreateInstance( &CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                     &IID_IFileOpenDialog, (void **)&dialog ))) goto done;
    if (FAILED(hr = IFileOpenDialog_QueryInterface( dialog, &IID_IFileDialog,
                                                   (void **)&file_dialog ))) goto done;

    if (FAILED(hr = IFileDialog_SetFileTypes( file_dialog, filters.count, filters.specs ))) goto done;
    if (FAILED(hr = IFileDialog_SetFileTypeIndex( file_dialog, filters.count ))) goto done;
    if (FAILED(hr = IFileDialog_GetOptions( file_dialog, &options ))) goto done;
    if (FAILED(hr = IFileDialog_SetOptions( file_dialog, options | FOS_FORCEFILESYSTEM |
                                           FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST |
                                           FOS_ALLOWMULTISELECT ))) goto done;

    commit = WindowsGetStringRawBuffer( operation->commit_button_text, &commit_length );
    if (commit_length && FAILED(hr = IFileDialog_SetOkButtonLabel( file_dialog, commit ))) goto done;
    set_default_folder( file_dialog, operation->location );

    hr = IFileDialog_Show( file_dialog, operation->hwnd );
    if (hr == HRESULT_FROM_WIN32( ERROR_CANCELLED ))
    {
        hr = pick_file_result_vector_create( NULL, 0, result );
        goto done;
    }
    if (FAILED(hr)) goto done;
    if (FAILED(hr = IFileOpenDialog_GetResults( dialog, &item_array ))) goto done;
    if (FAILED(hr = IShellItemArray_GetCount( item_array, &count ))) goto done;
    if (count && !(items = calloc( count, sizeof(*items) )))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }

    for (i = 0; i < count; ++i)
    {
        if (FAILED(hr = IShellItemArray_GetItemAt( item_array, i, &item ))) goto done;
        if (FAILED(hr = IShellItem_GetDisplayName( item, SIGDN_FILESYSPATH, &path ))) goto done;
        if (FAILED(hr = pick_file_result_create( path, &items[created] ))) goto done;
        ++created;
        CoTaskMemFree( path );
        path = NULL;
        IShellItem_Release( item );
        item = NULL;
    }
    if (SUCCEEDED(hr = pick_file_result_vector_create( items, count, result ))) items = NULL;

done:
    CoTaskMemFree( path );
    if (item) IShellItem_Release( item );
    if (items)
    {
        for (i = 0; i < created; ++i) IPickFileResult_Release( items[i] );
        free( items );
    }
    if (item_array) IShellItemArray_Release( item_array );
    if (file_dialog) IFileDialog_Release( file_dialog );
    if (dialog) IFileOpenDialog_Release( dialog );
    dialog_filters_destroy( &filters );
    return hr;
}

static void operation_complete( struct file_picker_operation *operation, HRESULT hr, IPickFileResult *result )
{
    IAsyncOperationCompletedHandler_PickFileResult *handler = NULL;
    AsyncStatus status;

    EnterCriticalSection( &operation->cs );
    if (operation->status == Started)
    {
        if (operation->cancel_requested)
        {
            operation->error = E_ABORT;
            operation->status = Canceled;
        }
        else
        {
            operation->error = hr;
            operation->status = FAILED(hr) ? Error : Completed;
            operation->result = result;
            result = NULL;
        }
    }
    status = operation->status;
    if (operation->handler)
    {
        handler = operation->handler;
        operation->handler = NULL;
    }
    LeaveCriticalSection( &operation->cs );

    if (result) IPickFileResult_Release( result );
    if (handler)
    {
        IAsyncOperationCompletedHandler_PickFileResult_Invoke(
            handler, &operation->IAsyncOperation_PickFileResult_iface, status );
        IAsyncOperationCompletedHandler_PickFileResult_Release( handler );
    }
}

static HRESULT file_picker_operation_create( struct file_open_picker *picker,
                                             IAsyncOperation_PickFileResult **out )
{
    struct file_picker_operation *operation;
    IPickFileResult *result = NULL;
    UINT32 i;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(operation = calloc( 1, sizeof(*operation) ))) return E_OUTOFMEMORY;
    operation->IAsyncOperation_PickFileResult_iface.lpVtbl = &operation_vtbl;
    operation->IAsyncInfo_iface.lpVtbl = &async_info_vtbl;
    operation->ref = 1;
    operation->status = Started;
    operation->error = S_OK;
    operation->hwnd = picker->hwnd;
    operation->location = picker->location;
    InitializeCriticalSectionEx( &operation->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    operation->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": file_picker_operation.cs");

    if (FAILED(hr = WindowsDuplicateString( picker->commit_button_text, &operation->commit_button_text ))) goto failed;
    if (FAILED(hr = IVector_HSTRING_get_Size( picker->filters, &operation->filter_count ))) goto failed;
    if (operation->filter_count &&
        !(operation->filters = calloc( operation->filter_count, sizeof(*operation->filters) )))
    {
        hr = E_OUTOFMEMORY;
        goto failed;
    }
    for (i = 0; i < operation->filter_count; ++i)
        if (FAILED(hr = IVector_HSTRING_GetAt( picker->filters, i, &operation->filters[i] ))) goto failed;

    hr = show_file_dialog( operation, &result );
    operation_complete( operation, hr, result );

    *out = &operation->IAsyncOperation_PickFileResult_iface;
    return S_OK;

failed:
    operation_release( operation );
    return hr;
}

static HRESULT save_file_picker_operation_create( struct file_save_picker *picker,
                                                  IAsyncOperation_PickFileResult **out )
{
    struct file_picker_operation *operation;
    IPickFileResult *result = NULL;
    UINT32 count, i, j, index = 0;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(operation = calloc( 1, sizeof(*operation) ))) return E_OUTOFMEMORY;
    operation->IAsyncOperation_PickFileResult_iface.lpVtbl = &operation_vtbl;
    operation->IAsyncInfo_iface.lpVtbl = &async_info_vtbl;
    operation->ref = 1;
    operation->status = Started;
    operation->error = S_OK;
    operation->hwnd = picker->hwnd;
    operation->location = picker->location;
    operation->save = TRUE;
    InitializeCriticalSectionEx( &operation->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    operation->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": save_file_picker_operation.cs");

    if (FAILED(hr = WindowsDuplicateString( picker->commit_button_text,
                                            &operation->commit_button_text ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( picker->default_extension,
                                            &operation->default_extension ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( picker->suggested_file_name,
                                            &operation->suggested_file_name ))) goto failed;
    if (FAILED(hr = WindowsDuplicateString( picker->suggested_folder,
                                            &operation->suggested_folder ))) goto failed;

    for (i = 0; i < picker->choices->size; ++i)
    {
        if (FAILED(hr = IVector_HSTRING_get_Size( picker->choices->entries[i].extensions,
                                                  &count ))) goto failed;
        if (count > ~(UINT32)0 - operation->filter_count)
        {
            hr = E_OUTOFMEMORY;
            goto failed;
        }
        operation->filter_count += count;
    }
    if (operation->filter_count &&
        !(operation->filters = calloc( operation->filter_count, sizeof(*operation->filters) )))
    {
        hr = E_OUTOFMEMORY;
        goto failed;
    }
    for (i = 0; i < picker->choices->size; ++i)
    {
        if (FAILED(hr = IVector_HSTRING_get_Size( picker->choices->entries[i].extensions,
                                                  &count ))) goto failed;
        for (j = 0; j < count; ++j)
            if (FAILED(hr = IVector_HSTRING_GetAt( picker->choices->entries[i].extensions, j,
                                                   &operation->filters[index++] ))) goto failed;
    }

    hr = show_file_dialog( operation, &result );
    operation_complete( operation, hr, result );
    *out = &operation->IAsyncOperation_PickFileResult_iface;
    return S_OK;

failed:
    operation_release( operation );
    return hr;
}

static BOOL multi_operation_is_canceled( struct multi_file_picker_operation *operation )
{
    BOOL canceled;
    EnterCriticalSection( &operation->cs );
    canceled = operation->cancel_requested;
    LeaveCriticalSection( &operation->cs );
    return canceled;
}

static void multi_operation_complete( struct multi_file_picker_operation *operation,
                                      HRESULT hr, IVectorView_PickFileResult *result )
{
    IAsyncOperationCompletedHandler_IVectorView_PickFileResult *handler = NULL;
    AsyncStatus status;

    EnterCriticalSection( &operation->cs );
    if (operation->status == Started)
    {
        if (operation->cancel_requested)
        {
            operation->error = E_ABORT;
            operation->status = Canceled;
        }
        else
        {
            operation->error = hr;
            operation->status = FAILED(hr) ? Error : Completed;
            operation->result = result;
            result = NULL;
        }
    }
    status = operation->status;
    if (operation->handler)
    {
        handler = operation->handler;
        operation->handler = NULL;
    }
    LeaveCriticalSection( &operation->cs );

    if (result) IVectorView_PickFileResult_Release( result );
    if (handler)
    {
        IAsyncOperationCompletedHandler_IVectorView_PickFileResult_Invoke(
            handler, &operation->IAsyncOperation_IVectorView_PickFileResult_iface, status );
        IAsyncOperationCompletedHandler_IVectorView_PickFileResult_Release( handler );
    }
}

static DWORD WINAPI multi_file_picker_worker( void *param )
{
    struct multi_file_picker_operation *operation = param;
    IVectorView_PickFileResult *result = NULL;
    BOOL com_initialized = FALSE;
    HRESULT hr;

    if (multi_operation_is_canceled( operation ))
        hr = E_ABORT;
    else if (SUCCEEDED(hr = CoInitializeEx( NULL, COINIT_APARTMENTTHREADED )))
    {
        com_initialized = TRUE;
        hr = show_multi_file_dialog( operation, &result );
    }
    multi_operation_complete( operation, hr, result );
    if (com_initialized) CoUninitialize();
    multi_operation_release( operation );
    return 0;
}

static HRESULT multi_file_picker_operation_create(
    struct file_open_picker *picker, IAsyncOperation_IVectorView_PickFileResult **out )
{
    struct multi_file_picker_operation *operation;
    UINT32 i;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(operation = calloc( 1, sizeof(*operation) ))) return E_OUTOFMEMORY;
    operation->IAsyncOperation_IVectorView_PickFileResult_iface.lpVtbl = &multi_operation_vtbl;
    operation->IAsyncInfo_iface.lpVtbl = &multi_async_info_vtbl;
    operation->ref = 1;
    operation->status = Started;
    operation->error = S_OK;
    operation->hwnd = picker->hwnd;
    operation->location = picker->location;
    InitializeCriticalSectionEx( &operation->cs, 0, RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO );
    operation->cs.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": multi_file_picker_operation.cs");

    if (FAILED(hr = WindowsDuplicateString( picker->commit_button_text,
                                            &operation->commit_button_text ))) goto failed;
    if (FAILED(hr = IVector_HSTRING_get_Size( picker->filters,
                                             &operation->filter_count ))) goto failed;
    if (operation->filter_count &&
        !(operation->filters = calloc( operation->filter_count, sizeof(*operation->filters) )))
    {
        hr = E_OUTOFMEMORY;
        goto failed;
    }
    for (i = 0; i < operation->filter_count; ++i)
        if (FAILED(hr = IVector_HSTRING_GetAt( picker->filters, i,
                                              &operation->filters[i] ))) goto failed;

    multi_operation_add_ref( operation );
    if (!(operation->thread = CreateThread( NULL, 0, multi_file_picker_worker, operation, 0, NULL )))
    {
        hr = HRESULT_FROM_WIN32( GetLastError() );
        multi_operation_release( operation );
        goto failed;
    }

    *out = &operation->IAsyncOperation_IVectorView_PickFileResult_iface;
    return S_OK;

failed:
    multi_operation_release( operation );
    return hr;
}

static HRESULT WINAPI picker_QueryInterface( IFileOpenPicker *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_IFileOpenPicker ))
        return E_NOINTERFACE;
    IFileOpenPicker_AddRef( iface );
    *out = iface;
    return S_OK;
}

static ULONG WINAPI picker_AddRef( IFileOpenPicker *iface )
{
    return InterlockedIncrement( &picker_from_iface( iface )->ref );
}

static ULONG WINAPI picker_Release( IFileOpenPicker *iface )
{
    struct file_open_picker *picker = picker_from_iface( iface );
    ULONG ref = InterlockedDecrement( &picker->ref );
    if (!ref)
    {
        WindowsDeleteString( picker->commit_button_text );
        IVector_HSTRING_Release( picker->filters );
        free( picker );
    }
    return ref;
}

static HRESULT WINAPI picker_GetIids( IFileOpenPicker *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IFileOpenPicker};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI picker_GetRuntimeClassName( IFileOpenPicker *iface, HSTRING *name )
{
    return get_runtime_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_FileOpenPicker, name );
}

static HRESULT WINAPI picker_GetTrustLevel( IFileOpenPicker *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI picker_get_ViewMode( IFileOpenPicker *iface, PickerViewMode *value )
{
    if (!value) return E_POINTER;
    *value = picker_from_iface( iface )->view_mode;
    return S_OK;
}

static HRESULT WINAPI picker_put_ViewMode( IFileOpenPicker *iface, PickerViewMode value )
{
    if (value != PickerViewMode_List && value != PickerViewMode_Thumbnail) return E_INVALIDARG;
    picker_from_iface( iface )->view_mode = value;
    return S_OK;
}

static BOOL valid_location( PickerLocationId value )
{
    switch (value)
    {
    case PickerLocationId_DocumentsLibrary:
    case PickerLocationId_ComputerFolder:
    case PickerLocationId_Desktop:
    case PickerLocationId_Downloads:
    case PickerLocationId_MusicLibrary:
    case PickerLocationId_PicturesLibrary:
    case PickerLocationId_VideosLibrary:
    case PickerLocationId_Objects3D:
    case PickerLocationId_Unspecified:
        return TRUE;
    default:
        return FALSE;
    }
}

static HRESULT replace_hstring( HSTRING value, HSTRING *destination )
{
    const WCHAR *buffer = WindowsGetStringRawBuffer( value, NULL );
    UINT32 length, i;
    HSTRING copy;
    HRESULT hr;

    WindowsGetStringRawBuffer( value, &length );
    for (i = 0; i < length; ++i) if (!buffer[i]) return E_INVALIDARG;
    if (FAILED(hr = WindowsDuplicateString( value, &copy ))) return hr;
    WindowsDeleteString( *destination );
    *destination = copy;
    return S_OK;
}

static HRESULT WINAPI picker_get_SuggestedStartLocation( IFileOpenPicker *iface, PickerLocationId *value )
{
    if (!value) return E_POINTER;
    *value = picker_from_iface( iface )->location;
    return S_OK;
}

static HRESULT WINAPI picker_put_SuggestedStartLocation( IFileOpenPicker *iface, PickerLocationId value )
{
    if (!valid_location( value )) return E_INVALIDARG;
    picker_from_iface( iface )->location = value;
    return S_OK;
}

static HRESULT WINAPI picker_get_CommitButtonText( IFileOpenPicker *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( picker_from_iface( iface )->commit_button_text, value );
}

static HRESULT WINAPI picker_put_CommitButtonText( IFileOpenPicker *iface, HSTRING value )
{
    return replace_hstring( value, &picker_from_iface( iface )->commit_button_text );
}

static HRESULT WINAPI picker_get_FileTypeFilter( IFileOpenPicker *iface, IVector_HSTRING **value )
{
    struct file_open_picker *picker = picker_from_iface( iface );
    if (!value) return E_POINTER;
    IVector_HSTRING_AddRef( *value = picker->filters );
    return S_OK;
}

static HRESULT WINAPI picker_PickSingleFileAsync( IFileOpenPicker *iface,
                                                  IAsyncOperation_PickFileResult **operation )
{
    return file_picker_operation_create( picker_from_iface( iface ), operation );
}

static HRESULT WINAPI picker_PickMultipleFilesAsync( IFileOpenPicker *iface,
    IAsyncOperation_IVectorView_PickFileResult **operation )
{
    return multi_file_picker_operation_create( picker_from_iface( iface ), operation );
}

static const IFileOpenPickerVtbl picker_vtbl =
{
    picker_QueryInterface,
    picker_AddRef,
    picker_Release,
    picker_GetIids,
    picker_GetRuntimeClassName,
    picker_GetTrustLevel,
    picker_get_ViewMode,
    picker_put_ViewMode,
    picker_get_SuggestedStartLocation,
    picker_put_SuggestedStartLocation,
    picker_get_CommitButtonText,
    picker_put_CommitButtonText,
    picker_get_FileTypeFilter,
    picker_PickSingleFileAsync,
    picker_PickMultipleFilesAsync,
};

static HRESULT file_open_picker_create( WindowId window_id, IFileOpenPicker **out )
{
    struct file_open_picker *picker;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(picker = calloc( 1, sizeof(*picker) ))) return E_OUTOFMEMORY;
    picker->IFileOpenPicker_iface.lpVtbl = &picker_vtbl;
    picker->ref = 1;
    picker->hwnd = (HWND)(ULONG_PTR)window_id.Value;
    picker->view_mode = PickerViewMode_List;
    picker->location = PickerLocationId_Unspecified;
    if (FAILED(hr = hstring_vector_create( &picker->filters )))
    {
        free( picker );
        return hr;
    }
    *out = &picker->IFileOpenPicker_iface;
    return S_OK;
}

static HRESULT WINAPI save_picker_QueryInterface( IFileSavePicker *iface, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID( iid, &IID_IUnknown ) && !IsEqualGUID( iid, &IID_IInspectable ) &&
        !IsEqualGUID( iid, &IID_IAgileObject ) && !IsEqualGUID( iid, &IID_IFileSavePicker ))
        return E_NOINTERFACE;
    IFileSavePicker_AddRef( iface );
    *out = iface;
    return S_OK;
}

static ULONG WINAPI save_picker_AddRef( IFileSavePicker *iface )
{
    return InterlockedIncrement( &save_picker_from_iface( iface )->ref );
}

static ULONG WINAPI save_picker_Release( IFileSavePicker *iface )
{
    struct file_save_picker *picker = save_picker_from_iface( iface );
    ULONG ref = InterlockedDecrement( &picker->ref );

    if (!ref)
    {
        WindowsDeleteString( picker->commit_button_text );
        IMap_HSTRING_IInspectable_Release( &picker->choices->IMap_HSTRING_IInspectable_iface );
        WindowsDeleteString( picker->default_extension );
        WindowsDeleteString( picker->suggested_file_name );
        WindowsDeleteString( picker->suggested_folder );
        free( picker );
    }
    return ref;
}

static HRESULT WINAPI save_picker_GetIids( IFileSavePicker *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IFileSavePicker};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI save_picker_GetRuntimeClassName( IFileSavePicker *iface, HSTRING *name )
{
    return get_runtime_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_FileSavePicker, name );
}

static HRESULT WINAPI save_picker_GetTrustLevel( IFileSavePicker *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI save_picker_get_SuggestedStartLocation( IFileSavePicker *iface,
                                                              PickerLocationId *value )
{
    if (!value) return E_POINTER;
    *value = save_picker_from_iface( iface )->location;
    return S_OK;
}

static HRESULT WINAPI save_picker_put_SuggestedStartLocation( IFileSavePicker *iface,
                                                              PickerLocationId value )
{
    if (!valid_location( value )) return E_INVALIDARG;
    save_picker_from_iface( iface )->location = value;
    return S_OK;
}

static HRESULT WINAPI save_picker_get_CommitButtonText( IFileSavePicker *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( save_picker_from_iface( iface )->commit_button_text, value );
}

static HRESULT WINAPI save_picker_put_CommitButtonText( IFileSavePicker *iface, HSTRING value )
{
    return replace_hstring( value, &save_picker_from_iface( iface )->commit_button_text );
}

static HRESULT WINAPI save_picker_get_FileTypeChoices( IFileSavePicker *iface,
                                                       IInspectable **value )
{
    struct file_save_picker *picker = save_picker_from_iface( iface );

    if (!value) return E_POINTER;
    IMap_HSTRING_IInspectable_AddRef( &picker->choices->IMap_HSTRING_IInspectable_iface );
    *value = (IInspectable *)&picker->choices->IMap_HSTRING_IInspectable_iface;
    return S_OK;
}

static HRESULT WINAPI save_picker_get_DefaultFileExtension( IFileSavePicker *iface,
                                                            HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( save_picker_from_iface( iface )->default_extension, value );
}

static HRESULT WINAPI save_picker_put_DefaultFileExtension( IFileSavePicker *iface, HSTRING value )
{
    return replace_hstring( value, &save_picker_from_iface( iface )->default_extension );
}

static HRESULT WINAPI save_picker_get_SuggestedFileName( IFileSavePicker *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( save_picker_from_iface( iface )->suggested_file_name, value );
}

static HRESULT WINAPI save_picker_put_SuggestedFileName( IFileSavePicker *iface, HSTRING value )
{
    return replace_hstring( value, &save_picker_from_iface( iface )->suggested_file_name );
}

static HRESULT WINAPI save_picker_get_SuggestedFolder( IFileSavePicker *iface, HSTRING *value )
{
    if (!value) return E_POINTER;
    return WindowsDuplicateString( save_picker_from_iface( iface )->suggested_folder, value );
}

static HRESULT WINAPI save_picker_put_SuggestedFolder( IFileSavePicker *iface, HSTRING value )
{
    return replace_hstring( value, &save_picker_from_iface( iface )->suggested_folder );
}

static HRESULT WINAPI save_picker_PickSaveFileAsync( IFileSavePicker *iface,
                                                     IAsyncOperation_PickFileResult **operation )
{
    return save_file_picker_operation_create( save_picker_from_iface( iface ), operation );
}

static const IFileSavePickerVtbl save_picker_vtbl =
{
    save_picker_QueryInterface,
    save_picker_AddRef,
    save_picker_Release,
    save_picker_GetIids,
    save_picker_GetRuntimeClassName,
    save_picker_GetTrustLevel,
    save_picker_get_SuggestedStartLocation,
    save_picker_put_SuggestedStartLocation,
    save_picker_get_CommitButtonText,
    save_picker_put_CommitButtonText,
    save_picker_get_FileTypeChoices,
    save_picker_get_DefaultFileExtension,
    save_picker_put_DefaultFileExtension,
    save_picker_get_SuggestedFileName,
    save_picker_put_SuggestedFileName,
    save_picker_get_SuggestedFolder,
    save_picker_put_SuggestedFolder,
    save_picker_PickSaveFileAsync,
};

static HRESULT file_save_picker_create( WindowId window_id, IFileSavePicker **out )
{
    struct file_save_picker *picker;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!(picker = calloc( 1, sizeof(*picker) ))) return E_OUTOFMEMORY;
    picker->IFileSavePicker_iface.lpVtbl = &save_picker_vtbl;
    picker->ref = 1;
    picker->hwnd = (HWND)(ULONG_PTR)window_id.Value;
    picker->location = PickerLocationId_Unspecified;
    if (FAILED(hr = file_type_choices_create( &picker->choices )))
    {
        free( picker );
        return hr;
    }
    *out = &picker->IFileSavePicker_iface;
    return S_OK;
}

static HRESULT factory_query_interface( struct file_open_picker_factory *factory, REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &factory->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IFileOpenPickerFactory ))
        *out = &factory->IFileOpenPickerFactory_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &factory->ref );
    return S_OK;
}

static ULONG factory_add_ref( struct file_open_picker_factory *factory )
{
    return InterlockedIncrement( &factory->ref );
}

static ULONG factory_release( struct file_open_picker_factory *factory )
{
    return InterlockedDecrement( &factory->ref );
}

static HRESULT WINAPI activation_QueryInterface( IActivationFactory *iface, REFIID iid, void **out )
{
    return factory_query_interface( factory_from_activation( iface ), iid, out );
}

static ULONG WINAPI activation_AddRef( IActivationFactory *iface )
{
    return factory_add_ref( factory_from_activation( iface ) );
}

static ULONG WINAPI activation_Release( IActivationFactory *iface )
{
    return factory_release( factory_from_activation( iface ) );
}

static HRESULT WINAPI activation_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IActivationFactory, &IID_IFileOpenPickerFactory};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI activation_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    return get_runtime_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_FileOpenPicker, name );
}

static HRESULT WINAPI activation_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI activation_ActivateInstance( IActivationFactory *iface, IInspectable **instance )
{
    if (instance) *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl activation_vtbl =
{
    activation_QueryInterface,
    activation_AddRef,
    activation_Release,
    activation_GetIids,
    activation_GetRuntimeClassName,
    activation_GetTrustLevel,
    activation_ActivateInstance,
};

static HRESULT WINAPI picker_factory_QueryInterface( IFileOpenPickerFactory *iface, REFIID iid, void **out )
{
    return factory_query_interface( factory_from_picker_factory( iface ), iid, out );
}

static ULONG WINAPI picker_factory_AddRef( IFileOpenPickerFactory *iface )
{
    return factory_add_ref( factory_from_picker_factory( iface ) );
}

static ULONG WINAPI picker_factory_Release( IFileOpenPickerFactory *iface )
{
    return factory_release( factory_from_picker_factory( iface ) );
}

static HRESULT WINAPI picker_factory_GetIids( IFileOpenPickerFactory *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IActivationFactory, &IID_IFileOpenPickerFactory};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI picker_factory_GetRuntimeClassName( IFileOpenPickerFactory *iface, HSTRING *name )
{
    return get_runtime_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_FileOpenPicker, name );
}

static HRESULT WINAPI picker_factory_GetTrustLevel( IFileOpenPickerFactory *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI picker_factory_CreateInstance( IFileOpenPickerFactory *iface, WindowId window_id,
                                                     IFileOpenPicker **value )
{
    return file_open_picker_create( window_id, value );
}

static const IFileOpenPickerFactoryVtbl picker_factory_vtbl =
{
    picker_factory_QueryInterface,
    picker_factory_AddRef,
    picker_factory_Release,
    picker_factory_GetIids,
    picker_factory_GetRuntimeClassName,
    picker_factory_GetTrustLevel,
    picker_factory_CreateInstance,
};

static struct file_open_picker_factory factory =
{
    {&activation_vtbl},
    {&picker_factory_vtbl},
    1,
};

IActivationFactory *file_open_picker_factory = &factory.IActivationFactory_iface;

static HRESULT save_factory_query_interface( struct file_save_picker_factory *factory,
                                             REFIID iid, void **out )
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IInspectable ) ||
        IsEqualGUID( iid, &IID_IAgileObject ) || IsEqualGUID( iid, &IID_IActivationFactory ))
        *out = &factory->IActivationFactory_iface;
    else if (IsEqualGUID( iid, &IID_IFileSavePickerFactory ))
        *out = &factory->IFileSavePickerFactory_iface;
    else
        return E_NOINTERFACE;
    InterlockedIncrement( &factory->ref );
    return S_OK;
}

static ULONG save_factory_add_ref( struct file_save_picker_factory *factory )
{
    return InterlockedIncrement( &factory->ref );
}

static ULONG save_factory_release( struct file_save_picker_factory *factory )
{
    return InterlockedDecrement( &factory->ref );
}

static HRESULT WINAPI save_activation_QueryInterface( IActivationFactory *iface, REFIID iid,
                                                      void **out )
{
    return save_factory_query_interface( save_factory_from_activation( iface ), iid, out );
}

static ULONG WINAPI save_activation_AddRef( IActivationFactory *iface )
{
    return save_factory_add_ref( save_factory_from_activation( iface ) );
}

static ULONG WINAPI save_activation_Release( IActivationFactory *iface )
{
    return save_factory_release( save_factory_from_activation( iface ) );
}

static HRESULT WINAPI save_activation_GetIids( IActivationFactory *iface, ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IActivationFactory, &IID_IFileSavePickerFactory};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI save_activation_GetRuntimeClassName( IActivationFactory *iface, HSTRING *name )
{
    return get_runtime_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_FileSavePicker, name );
}

static HRESULT WINAPI save_activation_GetTrustLevel( IActivationFactory *iface, TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI save_activation_ActivateInstance( IActivationFactory *iface,
                                                        IInspectable **instance )
{
    if (instance) *instance = NULL;
    return E_NOTIMPL;
}

static const IActivationFactoryVtbl save_activation_vtbl =
{
    save_activation_QueryInterface,
    save_activation_AddRef,
    save_activation_Release,
    save_activation_GetIids,
    save_activation_GetRuntimeClassName,
    save_activation_GetTrustLevel,
    save_activation_ActivateInstance,
};

static HRESULT WINAPI save_picker_factory_QueryInterface( IFileSavePickerFactory *iface,
                                                          REFIID iid, void **out )
{
    return save_factory_query_interface( save_factory_from_picker_factory( iface ), iid, out );
}

static ULONG WINAPI save_picker_factory_AddRef( IFileSavePickerFactory *iface )
{
    return save_factory_add_ref( save_factory_from_picker_factory( iface ) );
}

static ULONG WINAPI save_picker_factory_Release( IFileSavePickerFactory *iface )
{
    return save_factory_release( save_factory_from_picker_factory( iface ) );
}

static HRESULT WINAPI save_picker_factory_GetIids( IFileSavePickerFactory *iface,
                                                   ULONG *count, IID **iids )
{
    static const IID *values[] = {&IID_IActivationFactory, &IID_IFileSavePickerFactory};
    return get_iids( count, iids, ARRAY_SIZE(values), values );
}

static HRESULT WINAPI save_picker_factory_GetRuntimeClassName( IFileSavePickerFactory *iface,
                                                               HSTRING *name )
{
    return get_runtime_class_name( RuntimeClass_Microsoft_Windows_Storage_Pickers_FileSavePicker, name );
}

static HRESULT WINAPI save_picker_factory_GetTrustLevel( IFileSavePickerFactory *iface,
                                                         TrustLevel *level )
{
    return get_trust_level( level );
}

static HRESULT WINAPI save_picker_factory_CreateInstance( IFileSavePickerFactory *iface,
                                                          WindowId window_id,
                                                          IFileSavePicker **value )
{
    return file_save_picker_create( window_id, value );
}

static const IFileSavePickerFactoryVtbl save_picker_factory_vtbl =
{
    save_picker_factory_QueryInterface,
    save_picker_factory_AddRef,
    save_picker_factory_Release,
    save_picker_factory_GetIids,
    save_picker_factory_GetRuntimeClassName,
    save_picker_factory_GetTrustLevel,
    save_picker_factory_CreateInstance,
};

static struct file_save_picker_factory save_factory =
{
    {&save_activation_vtbl},
    {&save_picker_factory_vtbl},
    1,
};

IActivationFactory *file_save_picker_factory = &save_factory.IActivationFactory_iface;
