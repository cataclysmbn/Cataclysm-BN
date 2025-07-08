#pragma once

template<typename T>
class string_id;

template<typename T>
class int_id;

template<typename Id>
struct to_string_id {
    using type = Id;
};

template<typename T>
struct to_string_id<int_id<T>> {
    using type = string_id<T>;
};

template<typename Id>
using to_string_id_t = typename to_string_id<Id>::type;


