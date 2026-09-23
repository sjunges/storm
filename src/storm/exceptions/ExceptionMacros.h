#pragma once

#include <exception>
#include <sstream>

/*!
 * Exception classes are fully defined in the header (no out-of-line key function), so their typeinfo and vtable are
 * emitted as weak symbols in every translation unit that uses them. To allow catching these exceptions across shared
 * library boundaries even when libraries are compiled with hidden visibility, they must always be compiled with
 * default visibility.
 */
#ifdef _MSC_VER
#define STORM_EXCEPTION_EXPORT_ATTRIBUTE
#elif defined(__GNUC__) || defined(__clang__)
#define STORM_EXCEPTION_EXPORT_ATTRIBUTE __attribute__((visibility("default")))
#else
#define STORM_EXCEPTION_EXPORT_ATTRIBUTE
#endif

/*!
 * Macro to generate descendant exception classes. As all classes are nearly the same, this makes changing common
 * features much easier.
 */
#define STORM_NEW_EXCEPTION(exception_name)                             \
    class STORM_EXCEPTION_EXPORT_ATTRIBUTE exception_name : public BaseException { \
       public:                                                          \
        exception_name() : BaseException() {}                           \
        exception_name(char const* cstr) : BaseException(cstr) {}       \
        exception_name(exception_name const& cp) : BaseException(cp) {} \
        ~exception_name() throw() {}                                    \
        virtual std::string type() const override {                     \
            return #exception_name;                                     \
        }                                                               \
        template<typename T>                                            \
        exception_name& operator<<(T const& var) {                      \
            this->stream << var;                                        \
            return *this;                                               \
        }                                                               \
    };
