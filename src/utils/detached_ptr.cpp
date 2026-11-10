#include "detached_ptr.h"

#include "item.h"
#include "location_ptr.h"

template <typename T> detached_ptr<T>::detached_ptr(): ptr(nullptr) {}

template <typename T> detached_ptr<T>::detached_ptr(detached_ptr&& source) noexcept {
    ptr = source.ptr;
    source.ptr = nullptr;
}

template <typename T>
auto detached_ptr<T>::operator=(detached_ptr&& source) noexcept -> detached_ptr<T>& {
    if (ptr) { ptr->destroy(); }
    ptr = source.ptr;
    source.ptr = nullptr;
    return *this;
}

template <typename T> detached_ptr<T>::detached_ptr(location_ptr<T, true>&& loc) {
    loc.unset_location();
    ptr = loc.ptr;
    loc.ptr = nullptr;
}

template <typename T> detached_ptr<T>::detached_ptr(location_ptr<T, false>&& loc) {
    loc.unset_location();
    ptr = loc.ptr;
    loc.ptr = nullptr;
}

template <typename T> detached_ptr<T>::~detached_ptr() {
    if (ptr) { ptr->destroy(); }
}

template <typename T> auto detached_ptr<T>::get() const -> T* {
    if (!*this) {
        debugmsg("Attempted to resolve invalid detached_ptr");
        return &null_item_reference();
    }
    return ptr;
}

template <typename T> detached_ptr<T>::operator bool() const { return !!*this; }

template <typename T> auto detached_ptr<T>::operator!() const -> bool { return !ptr; }

template <typename T> auto detached_ptr<T>::operator*() const -> T& { return *get(); }

template <typename T> auto detached_ptr<T>::operator->() const -> T* { return get(); }

template <typename T> auto detached_ptr<T>::operator==(const T& against) const -> bool {
    return ptr == &against;
}

template <typename T> auto detached_ptr<T>::operator==(const T* against) const -> bool {
    return ptr == against;
}

template <typename T>
template <typename U>
auto detached_ptr<T>::operator!=(const U against) const -> bool {
    return !(*this == against);
}

template <typename T> detached_ptr<T>::detached_ptr(T* obj) { ptr = obj; }
template <typename T> auto detached_ptr<T>::release() -> T* {
    T* ret = ptr;
    ptr = nullptr;
    return ret;
}

template class detached_ptr<item>;

template bool detached_ptr<item>::operator!=(const decltype(nullptr)) const;

template bool detached_ptr<item>::operator!=(item* const) const;
