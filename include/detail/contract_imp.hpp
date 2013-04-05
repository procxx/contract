#ifndef included_contract_imp_hpp__
#define included_contract_imp_hpp__

#include <type_traits>
#include <iostream>
#include <cstdlib>

// implementation: macros
//

#define contract_fun__                                                       \
    auto contract_obj__ =                                                    \
        contract::detail::contractor<void *>(0)                              \
        + [&](contract::detail::contract_context const & contract_context__) \

#define contract_meth__                                                      \
    auto contract_obj__ =                                                    \
        contract::detail::contractor<                                        \
            std::remove_reference<decltype(*this)>::type>(this)              \
        + [&](contract::detail::contract_context const & contract_context__) \

#define contract_ctor__                                                      \
    auto contract_obj__ =                                                    \
        contract::detail::contractor<                                        \
            std::remove_reference<decltype(*this)>::type>(                   \
                this, false, true)                                           \
        + [&](contract::detail::contract_context const & contract_context__) \

#define contract_dtor__                                                      \
    auto contract_obj__ =                                                    \
        contract::detail::contractor<                                        \
            std::remove_reference<decltype(*this)>::type>(                   \
                this, true, false)                                           \
        + [&](contract::detail::contract_context const & contract_context__) \

#define contract_class__                                                     \
    template <typename T>                                                    \
        friend class contract::detail::class_contract_base;                  \
                                                                             \
    template <typename T>                                                    \
    friend class contract::detail::has_class_contract;                       \
                                                                             \
    void class_contract__(                                                   \
        contract::detail::contract_context const & contract_context__) const \

#define contract_check__(TYPE, EXPR, MSG)                       \
    do {                                                        \
        if (contract_context__.check_ ## TYPE && !(EXPR))       \
            contract::handle_violation(contract::type:: TYPE,   \
                                       MSG,                     \
                                       #EXPR,                   \
                                       __func__,                \
                                       __FILE__,                \
                                       __LINE__);               \
    } while (0)                                                 \

// macros for variadic argument dispatch

#define arg_count__(...) arg_pos__(__VA_ARGS__, 5, 4, 3, 2, 1)
#define arg_pos__(                          _1,_2,_3,_4,_5, N, ...)  N

#define concat__(macro, argc)   concat2__(macro, argc)
#define concat2__(macro, argc)  macro ## argc

namespace contract
{
namespace detail
{

// implementation: code behind macros
//

struct contract_context
{
    contract_context(bool pre, bool post, bool inv)
        : check_precondition{pre}
        , check_postcondition{post}
        , check_invariant{inv}
    {}

    bool check_precondition;
    bool check_postcondition;
    bool check_invariant;
};

template <typename ContrFunc>
struct fun_contract
{
    explicit
    fun_contract(ContrFunc f, bool enter = true, bool exit = true)
        : contr_{f}
        , exit_{exit}
    {
        contr_(contract_context{true, false, enter});
    }

    ~fun_contract()
    {
        contr_(contract_context{false, true, exit_});
    }

    ContrFunc contr_;
    bool exit_;
};

template <typename T>
struct class_contract_base
{
    class_contract_base(T const * obj, bool enter, bool exit)
        : obj_{obj}
        , exit_{exit}
    {
        if (enter)
            obj_->class_contract__(contract_context{false, false, true});
    }

    ~class_contract_base()
    {
        if (exit_)
            obj_->class_contract__(contract_context{false, false, true});
    }

    T const * obj_;
    bool exit_;
};

template <typename T, typename ContrFunc>
struct class_contract
    : class_contract_base<T>
    , fun_contract<ContrFunc>
{
    using class_base = class_contract_base<T>;
    using fun_base = fun_contract<ContrFunc>;

    class_contract(T const * obj, ContrFunc f, bool enter, bool exit)
        : class_base{obj, enter, exit}
        , fun_base{f, enter, exit}
    {}
};

template <typename T>
struct has_class_contract
{
    template <typename U>
    static auto test(int) -> decltype(std::declval<U>().class_contract__(
                                          std::declval<contract_context>()),
                                      std::true_type{});
    template <typename U>
    static auto test(...) -> std::false_type;

    using type = decltype(test<T>(0));
};

template <typename T, bool = has_class_contract<T>::type::value>
struct contractor;

template <typename T>
struct contractor<T, false>
{
    explicit
    contractor(T const *, bool = true, bool = true) {}

    template <typename Func>
    fun_contract<Func> operator+(Func f) const
    {
        return fun_contract<Func>{f, true, true};
    }
};

template <typename T>
struct contractor<T, true>
{
    explicit
    contractor(T const * obj, bool enter = true, bool exit = true)
        : obj_{obj}
        , enter_{enter}
        , exit_{exit}
    {}

    template<typename Func>
    class_contract<T, Func> operator+(Func f) const
    {
        return class_contract<T, Func>{obj_, f, enter_, exit_};
    }

    T const * obj_;
    bool enter_;
    bool exit_;
};

// implementation: violation handler
//

inline
void default_handler(type contr_type,
                     char const * message,
                     char const * expr,
                     char const * func,
                     char const * file,
                     std::size_t line)
{
    std::cerr << file << ':' << line << ": error: "
              << "contract violation of type '";

    char const * type_str;

    switch (contr_type)
    {
    case type::precondition:
        type_str = "precondition";
        break;
    case type::postcondition:
        type_str = "postcondition";
        break;
    case type::invariant:
        type_str = "invariant";
        break;
    }

    std::cerr << type_str << "'\n"
              << "message:   " << message << "\n"
              << "condition: " << expr << "\n"
              << "function:  " << func << std::endl;

    std::abort();
}

// Holder for the currently installed contract failure handler.
// Templated with a dummy type to be able to keep it in the header file.
template <typename T = void>
struct handler_holder
{
    static
    violation_handler current_handler;
};

template <typename T>
violation_handler handler_holder<T>::current_handler{default_handler};

}  // namespace detail

inline
void handle_violation(type contr_type,
                      char const * message,
                      char const * expr,
                      char const * func,
                      char const * file,
                      std::size_t line)
{
    detail::handler_holder<>::current_handler(contr_type,
                                              message,
                                              expr,
                                              func,
                                              file,
                                              line);

    // if the handler returns, abort anyway to satisfy the [[noreturn]] contract
    std::abort();
}

inline
violation_handler set_handler(violation_handler new_handler)
{
    violation_handler old_handler = detail::handler_holder<>::current_handler;
    detail::handler_holder<>::current_handler = new_handler;
    return old_handler;
}

inline
violation_handler get_handler()
{
    return detail::handler_holder<>::current_handler;
}

}  // namespace contract

// Copyright Alexei Zakharov, 2013.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#endif
