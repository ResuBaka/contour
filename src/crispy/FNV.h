#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace crispy {

/**
 * This implements the FNV1a (Fowler–Noll–Vo) hash function.
 *
 * @see http://en.wikipedia.org/wiki/Fowler-Noll-Vo_hash_function
 */
template <typename T>
class FNV {
  public:
    constexpr FNV() noexcept : basis_{2166136261llu}, prime_{16777619llu} {}
    constexpr FNV(T prime, T basis) noexcept : basis_(basis), prime_(prime) {}

    /// Incrementally builds up an FNV hash by applying @p _value to already constructed @p _memory
    /// and returns the applied FNV result.
    constexpr unsigned operator()(unsigned _memory, T _value) const noexcept
    {
        _memory ^= _value;
        _memory *= prime_;
        return _memory;
    }

    /// Builds the FNV hash between [_begin, _end)
    constexpr unsigned operator()(unsigned _memory, std::basic_string<T> const& str) const noexcept
    {
        for (auto const ch : str)
            _memory = (*this)(_memory, ch);
        return _memory;
    }

    /// Builds the FNV hash between [_begin, _end)
    constexpr unsigned operator()(T const* _begin, T const* _end) const noexcept
    {
        auto memory = basis_;
        while (_begin != _end)
            memory = (*this)(memory, *_begin++);
        return memory;
    }

    /// Builds the FNV hash between [_begin, _begin + len)
    constexpr unsigned operator()(T const* data, size_t len) const noexcept
    {
        return (*this)(data, data + len);
    }

    /// Builds the FNV hash between [_begin, _begin + len)
    constexpr unsigned operator()(std::basic_string<T> const& _str) const noexcept
    {
        return (*this)(_str.data(), _str.data() + _str.size());
    }

  protected:
    unsigned const basis_;
    unsigned const prime_;
};

} // end namespace crispy
