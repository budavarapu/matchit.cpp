#ifndef _PATTERNS_H_
#define _PATTERNS_H_

#include <memory>
#include <tuple>
#include <functional>

namespace matchit
{
    namespace impl
    {
        template <typename Pattern>
        class PatternTraits;

        template <typename Value, typename Pattern>
        auto matchPattern(Value const &value, Pattern const &pattern)
            -> decltype(PatternTraits<Pattern>::matchPatternImpl(value, pattern))
        {
            return PatternTraits<Pattern>::matchPatternImpl(value, pattern);
        }

        template <typename Pattern>
        void resetId(Pattern const &pattern)
        {
            PatternTraits<Pattern>::resetIdImpl(pattern);
        }

        template <typename Pattern, typename Func>
        class PatternPair
        {
        public:
            using RetType = std::invoke_result_t<Func>;

            PatternPair(Pattern const &pattern, Func const &func)
                : mPattern{pattern}, mHandler{func}
            {
            }
            template <typename Value>
            bool matchValue(Value const &value) const
            {
                resetId(mPattern);
                return matchPattern(value, mPattern);
            }
            auto execute() const
            {
                return mHandler();
            }

        private:
            Pattern const &mPattern;
            Func const &mHandler;
        };

        template <typename Pattern, typename Pred>
        class PostCheck;

        template <typename Pattern>
        class PatternHelper
        {
        public:
            explicit PatternHelper(Pattern const &pattern)
                : mPattern{pattern}
            {
            }
            template <typename Func>
            auto operator=(Func const &func)
            {
                return PatternPair<Pattern, Func>{mPattern, func};
            }
            template <typename Pred>
            auto when(Pred const &pred)
            {
                return PatternHelper<PostCheck<Pattern, Pred> >(PostCheck(mPattern, pred));
            }

        private:
            Pattern const mPattern;
        };

        template <typename Pattern>
        auto pattern(Pattern const &p)
        {
            return PatternHelper<std::decay_t<Pattern> >{p};
        }

        template <typename... Patterns>
        class Ds;

        template <typename... Patterns>
        auto ds(Patterns const &...patterns) -> Ds<Patterns...>;

        template <typename First, typename... Patterns>
        auto pattern(First const &f, Patterns const &...ps)
        {
            return PatternHelper<Ds<First, Patterns...> >{ds(f, ps...)};
        }

        template <typename Pattern>
        class PatternTraits
        {
        public:
            template <typename Value>
            static auto matchPatternImpl(Value const &value, Pattern const &pattern)
                -> decltype(pattern == value)
            {
                return pattern == value;
            }
            static void resetIdImpl(Pattern const &)
            {
            }
        };

        class Wildcard
        {
        };

        constexpr Wildcard _;

        template <>
        class PatternTraits<Wildcard>
        {
            using Pattern = Wildcard;

        public:
            template <typename Value>
            static bool matchPatternImpl(Value const &, Pattern const &)
            {
                return true;
            }
            static void resetIdImpl(Pattern const &)
            {
            }
        };

        template <typename... Patterns>
        class Or
        {
        public:
            explicit Or(Patterns const &...patterns)
                : mPatterns{patterns...}
            {
            }
            auto const &patterns() const
            {
                return mPatterns;
            }

        private:
            std::tuple<Patterns...> mPatterns;
        };

        template <typename... Patterns>
        auto or_(Patterns const &...patterns)
        {
            return Or<Patterns...>{patterns...};
        }

        template <typename... Patterns>
        class PatternTraits<Or<Patterns...> >
        {
        public:
            template <typename Value>
            static auto matchPatternImpl(Value const &value, Or<Patterns...> const &orPat)
                -> decltype((matchPattern(value, std::declval<Patterns>()) || ...))
            {
                return std::apply(
                    [&value](Patterns const &...patterns) {
                        return (matchPattern(value, patterns) || ...);
                    },
                    orPat.patterns());
            }
            static void resetIdImpl(Or<Patterns...> const &orPat)
            {
                return std::apply(
                    [](Patterns const &...patterns) {
                        return (resetId(patterns), ...);
                    },
                    orPat.patterns());
            }
        };

        template <typename Pred>
        class Meet : public Pred
        {
        public:
            using Pred::operator();
        };

        template <typename Pred>
        auto meet(Pred const &pred)
        {
            return Meet<Pred>{pred};
        }

        template <typename Pred>
        class PatternTraits<Meet<Pred> >
        {
        public:
            template <typename Value>
            static auto matchPatternImpl(Value const &value, Meet<Pred> const &meetPat)
                -> decltype(meetPat(value))
            {
                return meetPat(value);
            }
            static void resetIdImpl(Meet<Pred> const &meetPat)
            {
            }
        };

        template <typename Unary, typename Pattern>
        class App
        {
        public:
            constexpr App(Unary const &unary, Pattern const &pattern)
                : mUnary{unary}, mPattern{pattern}
            {
            }
            auto const &unary() const
            {
                return mUnary;
            }
            auto const &pattern() const
            {
                return mPattern;
            }

        private:
            Unary const mUnary;
            Pattern const mPattern;
        };

        template <typename Unary, typename Pattern>
        constexpr auto app(Unary const &unary, Pattern const &pattern)
        {
            return App<Unary, Pattern>{unary, pattern};
        }

        template <typename Unary, typename Pattern>
        class PatternTraits<App<Unary, Pattern> >
        {
        public:
            template <typename Value>
            static auto matchPatternImpl(Value const &value, App<Unary, Pattern> const &appPat)
                -> decltype(matchPattern(std::invoke(appPat.unary(), value), appPat.pattern()))
            {
                return matchPattern(std::invoke(appPat.unary(), value), appPat.pattern());
            }
            static void resetIdImpl(App<Unary, Pattern> const &appPat)
            {
                return resetId(appPat.pattern());
            }
        };

        template <typename... Patterns>
        class And
        {
        public:
            explicit And(Patterns const &...patterns)
                : mPatterns{patterns...}
            {
            }
            auto const &patterns() const
            {
                return mPatterns;
            }

        private:
            std::tuple<Patterns...> mPatterns;
        };

        template <typename... Patterns>
        auto and_(Patterns const &...patterns)
        {
            return And<Patterns...>{patterns...};
        }

        template <typename... Patterns>
        class PatternTraits<And<Patterns...> >
        {
        public:
            template <typename Value>
            static auto matchPatternImpl(Value const &value, And<Patterns...> const &andPat)
                -> decltype((matchPattern(value, std::declval<Patterns>()) && ...))
            {
                return std::apply(
                    [&value](Patterns const &...patterns) {
                        return (matchPattern(value, patterns) && ...);
                    },
                    andPat.patterns());
            }
            static void resetIdImpl(And<Patterns...> const &andPat)
            {
                return std::apply(
                    [](Patterns const &...patterns) {
                        return (resetId(patterns), ...);
                    },
                    andPat.patterns());
            }
        };

        template <typename Pattern>
        class Not
        {
        public:
            explicit Not(Pattern const &pattern)
                : mPattern{pattern}
            {
            }
            auto const &pattern() const
            {
                return mPattern;
            }

        private:
            Pattern mPattern;
        };

        template <typename Pattern>
        auto not_(Pattern const &pattern)
        {
            return Not<Pattern>{pattern};
        }

        template <typename Pattern>
        class PatternTraits<Not<Pattern> >
        {
        public:
            template <typename Value>
            static auto matchPatternImpl(Value const &value, Not<Pattern> const &notPat)
                -> decltype(!matchPattern(value, notPat.pattern()))
            {
                return !matchPattern(value, notPat.pattern());
            }
            static void resetIdImpl(Not<Pattern> const &notPat)
            {
                resetId(notPat.pattern());
            }
        };

        template <bool own>
        class IdTrait;

        class Deleter
        {
        public:
            template <typename Type>
            void operator()(Type *ptr)
            {
                if (mOwn)
                {
                    std::default_delete<Type>{}(ptr);
                }
            }
            bool mOwn{false};
        };
        template <>
        class IdTrait<true>
        {
        public:
            template <typename Type, typename Value>
            static auto matchValueImpl(std::unique_ptr<Type, Deleter> &ptr, Value const &value)
                -> decltype(ptr = std::unique_ptr<Type, Deleter>(new Type{value}, Deleter{true}), void())
            {
                ptr = std::unique_ptr<Type, Deleter>(new Type{value}, Deleter{true});
            }
        };

        template <>
        class IdTrait<false>
        {
        public:
            template <typename Ptr, typename Value>
            static auto matchValueImpl(Ptr &ptr, Value const &value)
                -> decltype(ptr.reset(&value), void())
            {
                ptr.reset(&value);
            }
        };

        template <typename Type, bool own = true>
        class Id
        {
            using PtrT = std::unique_ptr<Type const, Deleter>;
            mutable std::shared_ptr<PtrT> mValue = std::make_shared<PtrT>();

        public:
            template <typename Value>
            auto matchValue(Value const &value) const
                -> decltype(**mValue == value, IdTrait<own>::matchValueImpl(*mValue, value), bool{})
            {
                if (*mValue)
                {
                    return **mValue == value;
                }
                IdTrait<own>::matchValueImpl(*mValue, value);
                return true;
            }
            void reset() const
            {
                (*mValue).reset();
            }
            Type const &value() const
            {
                return **mValue;
            }
            Type const &operator*() const
            {
                return value();
            }
        };

        template <typename Type>
        using RefId = Id<Type, false>;

        template <typename Type, bool own>
        class PatternTraits<Id<Type, own> >
        {
        public:
            template <typename Value>
            static auto matchPatternImpl(Value const &value, Id<Type, own> const &idPat)
                -> decltype(idPat.matchValue(value))
            {
                return idPat.matchValue(value);
            }
            static void resetIdImpl(Id<Type, own> const &idPat)
            {
                idPat.reset();
            }
        };

        template <typename... Patterns>
        class Ds
        {
        public:
            explicit Ds(Patterns const &...patterns)
                : mPatterns{patterns...}
            {
            }
            auto const &patterns() const
            {
                return mPatterns;
            }

        private:
            template <typename T>
            struct AddConstToPointer
            {
                using type = std::conditional_t<
                    !std::is_pointer_v<T>,
                    T,
                    std::add_pointer_t<std::add_const_t<std::remove_pointer_t<T> > > >;
            };
            template <typename T>
            using AddConstToPointerT = typename AddConstToPointer<T>::type;

            static_assert(std::is_same_v<AddConstToPointerT<void *>, void const *>);
            static_assert(std::is_same_v<AddConstToPointerT<int32_t>, int32_t>);

        public:
            using Type = std::tuple<AddConstToPointerT<std::decay_t<Patterns> >...>;

        private:
            Type mPatterns;
        };

        template <typename... Patterns>
        auto ds(Patterns const &...patterns) -> Ds<Patterns...>
        {
            return Ds<Patterns...>{patterns...};
        }

        template <typename Value, typename Pattern, typename = std::void_t<> >
        struct MatchFuncDefined : std::false_type
        {
        };

        template <typename Value, typename Pattern>
        struct MatchFuncDefined<Value, Pattern, std::void_t<decltype(matchPattern(std::declval<Value>(), std::declval<Pattern>()))> >
            : std::true_type
        {
        };

        template <typename Value, typename Pattern>
        inline constexpr bool MatchFuncDefinedV = MatchFuncDefined<Value, Pattern>::value;

        template <typename ValuesTuple, typename PatternsTuple>
        bool tryOooMatch(ValuesTuple const &values, PatternsTuple const &patterns);

        template <typename Pattern>
        class IsOoo;

        template <typename Pattern>
        inline constexpr bool isOooV = IsOoo<std::decay_t<Pattern> >::value;

        template <typename ValuesTuple, typename PatternsTuple, typename Enable = void>
        class TupleMatchHelper
        {
            template <typename VT = ValuesTuple>
            static bool tupleMatchImpl(VT const &values, PatternsTuple const &patterns) = delete;
        };

        using std::get;
        template <std::size_t N, typename Tuple, std::size_t... I>
        auto dropImpl(Tuple &&t, std::index_sequence<I...>)
        {
            // Fixme, use std::forward_as_tuple when possible.
            // return std::forward_as_tuple(get<I + N>(std::forward<Tuple>(t))...);
            return std::make_tuple(get<I + N>(std::forward<Tuple>(t))...);
        }

        // std::tuple_size_v cannot work with SFINAE with GCC, use std::tuple_size<>::value instead.
        template <std::size_t N, typename Tuple>
        auto drop(Tuple &&t)
            -> decltype(dropImpl<N>(
                std::forward<Tuple>(t),
                std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple> >::value - N>{}))
        {
            return dropImpl<N>(
                std::forward<Tuple>(t),
                std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple> >::value - N>{});
        }

        template <typename ValuesTuple, typename PatternHead, typename... PatternTail>
        class TupleMatchHelper<ValuesTuple, std::tuple<PatternHead, PatternTail...>, std::enable_if_t<!isOooV<PatternHead> > >
        {
        public:
            template <typename VT = ValuesTuple>
            static auto tupleMatchImpl(VT const &values, std::tuple<PatternHead, PatternTail...> const &patterns)
                -> decltype(matchPattern(get<0>(values), get<0>(patterns)) && TupleMatchHelper<decltype(drop<1>(values)), decltype(drop<1>(patterns))>::tupleMatchImpl(drop<1>(values), drop<1>(patterns)))
            {
                return matchPattern(get<0>(values), get<0>(patterns)) && TupleMatchHelper<decltype(drop<1>(values)), decltype(drop<1>(patterns))>::tupleMatchImpl(drop<1>(values), drop<1>(patterns));
            }
        };

        template <typename PatternHead, typename... PatternTail>
        class TupleMatchHelper<std::tuple<>, std::tuple<PatternHead, PatternTail...>, std::enable_if_t<!isOooV<PatternHead> > >
        {
        public:
            template <typename VT = std::tuple<> >
            static bool tupleMatchImpl(VT const &values, std::tuple<PatternHead, PatternTail...> const &patterns) = delete;
        };

        template <typename ValuesTuple>
        class TupleMatchHelper<ValuesTuple, std::tuple<> >
        {
        public:
            template <typename VT = std::tuple<> >
            static auto tupleMatchImpl(VT const &values, std::tuple<>)
            {
                return false;
            }
        };

        template <typename ValuesTuple, typename PatternHead, typename... PatternTail>
        class TupleMatchHelper<ValuesTuple, std::tuple<PatternHead, PatternTail...>, std::enable_if_t<isOooV<PatternHead> > >
        {
        public:
            template <typename VT = std::tuple<> >
            static auto tupleMatchImpl(VT const &values, std::tuple<PatternHead, PatternTail...> const &patterns)
                -> decltype(tryOooMatch(values, patterns))
            {
                return tryOooMatch(values, patterns);
            }
        };

        template <>
        class TupleMatchHelper<std::tuple<>, std::tuple<> >
        {
        public:
            template <typename VT = std::tuple<> >
            static auto tupleMatchImpl(VT, std::tuple<>)
            {
                return true;
            }
        };

        template <typename... Patterns>
        class PatternTraits<Ds<Patterns...> >
        {
        public:
            template <typename Tuple>
            static auto matchPatternImpl(Tuple const &valueTuple, Ds<Patterns...> const &dsPat)
                -> decltype(TupleMatchHelper<Tuple, typename Ds<Patterns...>::Type>::tupleMatchImpl(valueTuple, dsPat.patterns()))
            {
                return TupleMatchHelper<Tuple, typename Ds<Patterns...>::Type>::tupleMatchImpl(valueTuple, dsPat.patterns());
            }
            static void resetIdImpl(Ds<Patterns...> const &dsPat)
            {
                return std::apply(
                    [](auto &&...patterns) {
                        return (resetId(patterns), ...);
                    },
                    dsPat.patterns());
            }

        private:
        };

        template <typename Pattern>
        class Ooo;

        template <typename Pattern>
        class IsOoo : public std::false_type
        {
        };

        template <typename Pattern>
        class IsOoo<Ooo<Pattern> > : public std::true_type
        {
        };

        static_assert(isOooV<Ooo<int> > == true);
        static_assert(isOooV<Ooo<int &> > == true);
        static_assert(isOooV<Ooo<int const &> > == true);
        static_assert(isOooV<Ooo<int &&> > == true);
        static_assert(isOooV<int> == false);
        static_assert(isOooV<const Ooo<Wildcard> &> == true);

        template <typename Tuple, std::size_t... I>
        auto takeImpl(Tuple &&t, std::index_sequence<I...>)
        {
            return std::forward_as_tuple(get<I>(std::forward<Tuple>(t))...);
        }

        template <std::size_t N, typename Tuple>
        auto take(Tuple &&t)
        {
            return takeImpl(
                std::forward<Tuple>(t),
                std::make_index_sequence<N>{});
        }

        template <typename ValuesTuple, typename PatternsTuple>
        bool tryOooMatch(ValuesTuple const &values, PatternsTuple const &patterns)
        {
            if constexpr (std::tuple_size_v<PatternsTuple> == 0)
            {
                return std::tuple_size_v<ValuesTuple> == 0;
            }
            else if constexpr (isOooV<std::tuple_element_t<0, PatternsTuple> >)
            {
                auto index = std::make_index_sequence<std::tuple_size_v<ValuesTuple> + 1>{};
                return tryOooMatchImpl(values, patterns, index);
            }
            else if constexpr (std::tuple_size_v<ValuesTuple> >= 1)
            {
                if constexpr (MatchFuncDefinedV<std::tuple_element_t<0, ValuesTuple>, std::tuple_element_t<0, PatternsTuple> >)
                {
                    return matchPattern(get<0>(values), std::get<0>(patterns)) && tryOooMatch(drop<1>(values), drop<1>(patterns));
                }
            }
            return false;
        }

        class OooMatchBreak : public std::exception
        {
        };

        template <std::size_t I, typename ValuesTuple, typename PatternsTuple>
        bool tryOooMatchImplHelper(ValuesTuple const &values, PatternsTuple const &patterns)
        {
            if constexpr (I == 0)
            {
                return (tryOooMatch(values, drop<1>(patterns)));
            }
            else if constexpr (I > 0)
            {
                if constexpr (MatchFuncDefinedV<decltype(take<I>(values)), std::tuple_element_t<0, PatternsTuple> >)
                {
                    if (!PatternTraits<std::tuple_element_t<0, PatternsTuple> >::matchPatternImplSingle(get<I - 1>(values), get<0>(patterns)))
                    {
                        throw OooMatchBreak();
                    }

                    if (!tryOooMatch(drop<I>(values), drop<1>(patterns)))
                    {
                        return false;
                    }
                    return true;
                }
            }
            throw OooMatchBreak();
        }

        template <typename ValuesTuple, typename PatternsTuple, std::size_t... I>
        bool tryOooMatchImpl(ValuesTuple const &values, PatternsTuple const &patterns, std::index_sequence<I...>)
        {
            try
            {
                return ((tryOooMatchImplHelper<I>(values, patterns)) || ...);
            }
            catch (const OooMatchBreak &)
            {
                return false;
            }
        }

        template <typename Pattern>
        class Ooo
        {
        public:
            explicit Ooo(Pattern const &pattern)
                : mPattern{pattern}
            {
            }
            auto const &pattern() const
            {
                return mPattern;
            }

        private:
            Pattern mPattern;
        };

        template <typename Pattern>
        auto ooo(Pattern const &pattern)
        {
            return Ooo<Pattern>{pattern};
        }

        template <typename Pattern>
        class PatternTraits<Ooo<Pattern> >
        {
        public:
            template <typename... Values>
            static auto matchPatternImpl(std::tuple<Values...> const &valueTuple, Ooo<Pattern> const &oooPat)
                -> decltype((matchPattern(std::declval<Values>(), oooPat.pattern()) && ...))
            {
                return std::apply(
                    [&oooPat](Values const &...values) {
                        auto result = (matchPattern(values, oooPat.pattern()) && ...);
                        return result;
                    },
                    valueTuple);
            }
            template <typename Value>
            static auto matchPatternImplSingle(Value const &value, Ooo<Pattern> const &oooPat)
                -> decltype(matchPattern(value, oooPat.pattern()))
            {
                return matchPattern(value, oooPat.pattern());
            }
            static void resetIdImpl(Ooo<Pattern> const &oooPat)
            {
                resetId(oooPat.pattern());
            }
        };

        template <typename Pattern, typename Pred>
        class PostCheck
        {
        public:
            explicit PostCheck(Pattern const &pattern, Pred const &pred)
                : mPattern{pattern}, mPred{pred}
            {
            }
            bool check() const
            {
                return mPred();
            }
            auto const &pattern() const
            {
                return mPattern;
            }

        private:
            Pattern const mPattern;
            Pred const mPred;
        };

        template <typename Pattern, typename Pred>
        class PatternTraits<PostCheck<Pattern, Pred> >
        {
        public:
            template <typename Value>
            static auto matchPatternImpl(Value const &value, PostCheck<Pattern, Pred> const &postCheck)
                -> decltype(matchPattern(value, postCheck.pattern()) && postCheck.check())
            {
                return matchPattern(value, postCheck.pattern()) && postCheck.check();
            }
            static void resetIdImpl(PostCheck<Pattern, Pred> const &postCheck)
            {
                resetId(postCheck.pattern());
            }
        };

        static_assert(MatchFuncDefinedV<char[4], Id<const char *, true> >);
        static_assert(MatchFuncDefinedV<std::tuple<>, Wildcard>);
        static_assert(MatchFuncDefinedV<std::tuple<>, Ds<> >);
        static_assert(!MatchFuncDefinedV<std::tuple<>, Ds<int> >);

        static_assert(MatchFuncDefinedV<const std::tuple<char, std::tuple<char, char>, int> &,
                                        const Ds<char, Ds<char, Id<char, true> >, int> &>);
        static_assert(!MatchFuncDefinedV<const int &, const Ds<char, Ds<char, Id<char, true> >, int> &>);

        static_assert(MatchFuncDefinedV<char, char>);
        static_assert(MatchFuncDefinedV<int, char>);
        static_assert(MatchFuncDefinedV<char, int>);

        static_assert(MatchFuncDefinedV<std::tuple<char>, Ds<char> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, int, std::tuple<char, int> >,
                                        Ds<char, int, Ds<char, int> > >);
        static_assert(MatchFuncDefinedV<std::tuple<char, int, std::tuple<char, std::tuple<char, char>, int> >,
                                        Ds<char, int, Ds<char, Ds<char, char>, int> > >);
        static_assert(MatchFuncDefinedV<std::tuple<char, int, std::tuple<char, std::tuple<char, char>, int> >,
                                        Ds<char, int, Ds<char, Ds<char, Id<char, true> >, int> > >);
        static_assert(MatchFuncDefinedV<const std::tuple<char, std::tuple<char, char>, int> &,
                                        const Ds<char, Ds<char, char>, int> &>);
        static_assert(MatchFuncDefinedV<char &,
                                        Id<char, true> >);
        static_assert(MatchFuncDefinedV<const std::tuple<char, char> &,
                                        const Ds<char, Id<char, true> > &>);
        static_assert(MatchFuncDefinedV<const std::tuple<char, std::tuple<char, char> > &,
                                        const Ds<char, Ds<char, Id<char, true> > > &>);
        static_assert(MatchFuncDefinedV<const std::tuple<std::tuple<char, char>, int> &,
                                        const Ds<Ds<char, Id<char, true> >, int> &>);
        static_assert(MatchFuncDefinedV<const std::tuple<int, std::tuple<char, char>, int> &,
                                        const Ds<int, Ds<char, Id<char, true> >, int> &>);
        static_assert(MatchFuncDefinedV<const std::tuple<char, std::tuple<char, char>, char> &,
                                        const Ds<char, Ds<char, Id<char, true> >, char> &>);
        static_assert(MatchFuncDefinedV<std::tuple<int, std::tuple<int, int>, int>,
                                        Ds<int, Ds<int, Id<int, true> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<char, char>, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, char>, Ds<char, Id<char, true> > >);
        static_assert(MatchFuncDefinedV<std::tuple<int, std::tuple<char, char>, int>, Ds<int, Ds<char, Id<char, true> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int64_t>, Ds<char, Ds<char, Id<char, true> >, int64_t> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, long>, Ds<char, Ds<char, Id<char, true> >, long> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, unsigned>, Ds<char, Ds<char, Id<char, true> >, unsigned> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<char, Id<char, true> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<Id<char, true> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<char, RefId<char> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<char, Id<char, false> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<char, Id<char, true> >, unsigned> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<char, Id<char, true> >, Wildcard> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<char, Id<char, true> >, char> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, char>, Ds<char, Ds<char, Id<char, true> >, char> >);
        static_assert(MatchFuncDefinedV<std::tuple<std::tuple<char, std::tuple<char, char>, int> >, Ds<Ds<char, Ds<char, Id<char, true> >, int> > >);
        static_assert(MatchFuncDefinedV<std::tuple<char, char>, Ds<char, Id<char, true> > >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<int, Id<char, true> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<Wildcard, Id<char, true> >, int> >);
        static_assert(MatchFuncDefinedV<char, char>);
        static_assert(MatchFuncDefinedV<std::tuple<char, char>, Ds<char, char> >);
        static_assert(MatchFuncDefinedV<std::tuple<char>, Ds<char> >);
        static_assert(!MatchFuncDefinedV<std::tuple<char>, char>);
        static_assert(MatchFuncDefinedV<std::tuple<char>, Ooo<char> >);
        static_assert(MatchFuncDefinedV<std::tuple<char>, Wildcard>);
        static_assert(MatchFuncDefinedV<std::tuple<std::tuple<char, char>, int>, Ds<Ds<char, Id<char, true> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<bool, std::tuple<char, char>, int>, Ds<bool, Ds<char, Id<char, true> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<int, std::tuple<char, char>, int>, Ds<int, Ds<char, Id<char, true> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<int, std::tuple<char, char>, char>, Ds<int, Ds<char, Id<char, true> >, char> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, char>, Ds<char, Ds<char, Id<char, true> >, char> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char>, int>, Ds<char, Ds<char, Id<char, true> >, int> >);
        static_assert(MatchFuncDefinedV<std::tuple<char, std::tuple<char, char> >, Ds<char, Ds<char, Id<char, true> > > >);
        static_assert(MatchFuncDefinedV<std::tuple<int, int, int, int, int>, Ds<Ooo<int> > >);
    } // namespace impl

    // export symbols
    using impl::_;
    using impl::and_;
    using impl::app;
    using impl::ds;
    using impl::Id;
    using impl::matchPattern;
    using impl::meet;
    using impl::not_;
    using impl::ooo;
    using impl::or_;
    using impl::pattern;
    using impl::RefId;
} // namespace matchit

#endif // _PATTERNS_H_
