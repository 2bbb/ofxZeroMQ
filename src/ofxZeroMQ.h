//
//  ofxZeroMQ.h
//
//  Created by 2bit on 2020/01/20.
//

#ifndef ofxZeroMQ_h
#define ofxZeroMQ_h

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <type_traits>
#include <string>
#include <vector>
#include <set>
#include <tuple>
#include <thread>

#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "ofConstants.h"
#include "ofFileUtils.h"
#include "ofJson.h"

#include "ofLog.h"

#pragma mark - meta functions

namespace ofxZeroMQ {
    template <typename T>
    using get_type = typename T::type;

    using std::enable_if;

    template <bool b, typename T = void>
    using enable_if_t = get_type<enable_if<b, T>>;

    using std::conditional;

    template <bool b, typename T, typename F>
    using conditional_t = get_type<conditional<b, T, F>>;

    template <typename type>
    struct is_null_pointer : std::false_type {};

    template <>
    struct is_null_pointer<std::nullptr_t> : std::true_type {};
    
    template <typename ...>
    using void_t = void;
    
    template <bool b>
    using bool_constant = std::integral_constant<bool, b>;

    template <typename cond>
    struct negation : bool_constant<!bool(cond::value)> {};

    template <typename ... conditions>
    struct conjunction : std::true_type {};

    template <typename cond>
    struct conjunction<cond> : cond {};
    
    template <typename cond, typename ... conditions>
    struct conjunction<cond, conditions ...> : conditional_t<bool(cond::value), conjunction<conditions ...>, cond> {};

    template <typename ... conditions>
    struct disjunction : std::false_type {};

    template <typename cond>
    struct disjunction<cond> : cond {};
    
    template <typename cond, typename ... conditions>
    struct disjunction<cond, conditions ...> : conditional_t<bool(cond::value), cond, disjunction<conditions ...>> {};
};

#pragma mark - ZMQ Flags

namespace ofxZeroMQ {
    struct SendFlag {
        constexpr SendFlag(bool nonblocking = true, bool more = false)
        : nonblocking{nonblocking}
        , more{more}
        {};
        
        bool nonblocking{true};
        bool more{false};
        
        operator zmq::send_flags() const {
            return (more ? zmq::send_flags::sndmore
                         : zmq::send_flags::none)
                 | (nonblocking ? zmq::send_flags::dontwait
                                : zmq::send_flags::none);
        }
        operator int() const
        { return static_cast<int>(operator zmq::send_flags()); };
    }; // SendFlag
    
    static constexpr SendFlag SendFlagNone{false, false};
    static constexpr SendFlag SendFlagNonblocking{true, false};
    static constexpr SendFlag SendFlagMore{false, true};
    static constexpr SendFlag SendFlagNonblockingMore{true, true};

    struct ReceiveFlag {
        constexpr ReceiveFlag(bool nonblocking = true)
        : nonblocking{nonblocking}
        {};
        
        bool nonblocking{true};
        
        operator zmq::recv_flags() const {
            return nonblocking ? zmq::recv_flags::dontwait : zmq::recv_flags::none;
        }
        operator int() const
        { return static_cast<int>(operator zmq::recv_flags()); };
    }; // ReceiveFlag
    
    static constexpr ReceiveFlag ReceiveFlagNone{false};
    static constexpr ReceiveFlag ReceiveFlagNonblocking{true};
    
    namespace detail {
        template <typename ... args>
        using last_arg_is_SendFlag = std::is_same<
            decltype(std::get<sizeof...(args) - 1>(std::tuple<typename std::decay<args>::type ...>{})),
            SendFlag
        >;
        template <typename ... args>
        using last_arg_is_ReceiveFlag = std::is_same<
            decltype(std::get<sizeof...(args) - 1>(std::tuple<typename std::decay<args>::type ...>{})),
            ReceiveFlag
        >;
    }; // detail
}; // ofxZeroMQ

#pragma mark - adl converter foward decalaration

namespace ofxZeroMQ {
    template <typename type = void, typename sfinae = void>
    struct adl_converter;
};

#pragma mark - Message declaration

namespace ofxZeroMQ {
    namespace detail {
        template <typename type>
        constexpr std::size_t calc_size()
        { return sizeof(type); };

        template <typename type, typename other, typename ... types>
        constexpr std::size_t calc_size()
        { return sizeof(type) + calc_size<other, types ...>(); };
    };
    struct Message : zmq::message_t {
        using zmq::message_t::message_t;
        
        inline Message(const zmq::message_t &mom) {
            rebuild(mom.size());
            std::memcpy(data(), mom.data(), mom.size());
        }
        
        inline Message(zmq::message_t &&mom)
        { mom.move(*this); };
        
        inline Message(Message &&v) = default;

        template <typename type>
        inline Message(type &&v)
        { from(std::forward<type>(v)); };
        
#pragma mark - copy value from target

        template <typename type>
        std::size_t memCopyFrom(const type *v, std::size_t value_size, std::size_t offset = 0) {
            if(size() < offset + value_size) {
                ofLogWarning("ofxZeroMQMessage::memCopyFrom") << "range out of bounds. given value_size = " << value_size << ", offset = " << offset << ". but size is " << size();
                return 0ul;
            }
            std::memcpy((char *)data() + offset, v, value_size);
            return value_size;
        }
        
        template <typename type>
        auto copyFrom(const type &v, std::size_t offset = 0)
            -> enable_if_t<std::is_standard_layout<type>::value, std::size_t>
        {
            constexpr std::size_t value_size = sizeof(type);
            if(size() < offset + value_size) {
                ofLogWarning("ofxZeroMQMessage::copyFrom") << "range out of bounds. given value_size = " << value_size << ", offset = " << offset << ". but size is " << size();
                return 0ul;
            }
            std::memcpy((char *)data() + offset, &v, value_size);
            return value_size;
        }
        
        template <typename type, std::size_t array_size>
        auto copyFrom(const type (&v)[array_size], std::size_t offset = 0)
            -> enable_if_t<std::is_standard_layout<type>::value, std::size_t>
        {
            constexpr std::size_t value_size = sizeof(type);
            if(size() < offset + value_size * array_size) {
                ofLogWarning("ofxZeroMQMessage::copyFrom") << "range out of bounds.  given value_size = " << value_size << ", offset = " << offset << ", array_size = " << array_size << ". but size is " << size();
                return 0ul;
            }
            std::memcpy((char *)data() + offset, v, value_size * array_size);
            return value_size * array_size;
        }
        
        template <typename ... types>
        inline auto set(const types & ... vs)
            -> enable_if_t<conjunction<std::is_standard_layout<types> ...>::value, std::size_t>
        {
            rebuild(ofxZeroMQ::detail::calc_size<types ...>());
            return set_impl(0ul, vs ...);
        }
        
#pragma mark - copy value from target
        
        template <typename type>
        std::size_t memCopyTo(type *v, std::size_t value_size, std::size_t offset = 0) const
        {
            if(size() < offset + value_size) {
                ofLogWarning("ofxZeroMQMessage::memCopyTo") << "range out of bounds. given value_size = " << value_size << ", offset = " << offset << ". but size is " << size();
                return 0ul;
            }
            std::memcpy(v, (char *)data() + offset, value_size);
            return value_size;
        }
        
        template <typename type>
        auto copyTo(type &v, std::size_t offset = 0) const
            -> enable_if_t<std::is_standard_layout<type>::value, std::size_t>
        {
            constexpr std::size_t value_size = sizeof(type);
            if(size() < offset + value_size) {
                ofLogWarning("ofxZeroMQMessage::copyTo") << "range out of bounds. given value_size = " << value_size << ", offset = " << offset << ". but size is " << size();
                return 0ul;
            }
            std::memcpy(&v, (char *)data() + offset, value_size);
            return value_size;
        }

        template <typename type, std::size_t array_size>
        auto copyTo(type (&v)[array_size], std::size_t offset = 0) const
            -> enable_if_t<std::is_standard_layout<type>::value, std::size_t>
        {
            constexpr std::size_t value_size = sizeof(type);
            if(size() < offset + value_size * array_size) {
                ofLogWarning("ofxZeroMQMessage::copyTo") << "range out of bounds.  given value_size = " << value_size << ", offset = " << offset << ", array_size = " << array_size << ". but size is " << size();
                return 0ul;
            }
            std::memcpy(v, (char *)data() + offset, value_size * array_size);
            return value_size * array_size;
        }
        
        template <typename ... types>
        inline auto setTo(types & ... vs) const
            -> enable_if_t<conjunction<std::is_standard_layout<types> ...>::value, std::size_t>
        {
            return set_to_impl(0ul, vs ...);
        }

        // definition is below
        template <typename type>
        void from(type &&v);
        
        template <typename type>
        inline Message &operator=(type &&v)
        { from(std::forward<type>(v)); };
        
        // definition is below
        template <typename type>
        void to(type &v) const;
        
        template <typename type>
        inline type get() const {
            type v;
            to(v);
            return v;
        };

        template <typename type>
        inline operator type() const
        { return get<type>(); };
        
    private:
        template <typename type>
        inline auto set_impl(std::size_t cursor, const type &v)
            -> enable_if_t<conjunction<std::is_standard_layout<type>>::value, std::size_t>
        {
            return copyFrom(v, cursor);
        }

        template <typename type, typename ... types>
        inline auto set_impl(std::size_t cursor, const type &v, const types & ... vs)
            -> enable_if_t<conjunction<std::is_standard_layout<type>, std::is_standard_layout<types> ...>::value, std::size_t>
        {
            auto s = copyFrom(v, cursor);
            if(0ul < s) {
                return s + set_impl(cursor + s, vs ...);
            } else {
                return s;
            }
        }
        
        template <typename type>
        inline auto set_to_impl(std::size_t cursor, type &v) const
            -> enable_if_t<conjunction<std::is_standard_layout<type>>::value, std::size_t>
        {
            return copyTo(v, cursor);
        }

        template <typename type, typename ... types>
        inline auto set_to_impl(std::size_t cursor, type &v, types & ... vs) const
            -> enable_if_t<conjunction<std::is_standard_layout<type>, std::is_standard_layout<types> ...>::value, std::size_t>
        {
            auto s = copyTo(v, cursor);
            if(0ul < s) {
                return s + set_to_impl(cursor + s, vs ...);
            } else {
                return s;
            }
        }
    };
};

#pragma mark - type traits
namespace ofxZeroMQ {
    namespace detail {
        template <typename type>
        using enable_if_standard_layout = typename std::enable_if<
            std::is_standard_layout<type>::value
        >::type;
        template <typename type>
        using enable_if_not_standard_layout = typename std::enable_if<
            std::is_standard_layout<type>::value
        >::type;
    };
};

#pragma mark - fundamental convert functions

namespace ofxZeroMQ {
    namespace detail {
#pragma mark zmq::message_t
        inline static void to_zmq_message(ofxZeroMQ::Message &m,
                                          const zmq::message_t &data)
        {
            m.rebuild(data.size());
            m.memCopyFrom(data.data(), data.size());
        };
        inline static void to_zmq_message(ofxZeroMQ::Message &m,
                                          zmq::message_t &&data)
        {
            m = std::move(data);
        };

        
        inline static void from_zmq_message(const ofxZeroMQ::Message &m,
                                            zmq::message_t &data)
        {
            data.rebuild(m.size());
            std::memcpy(data.data(), m.data(), m.size());
        };
        
#pragma mark zmq::message_t
        inline static void to_zmq_message(ofxZeroMQ::Message &m,
                                          const ofxZeroMQ::Message &data)
        { to_zmq_message(m, static_cast<const zmq::message_t &>(data)); };

        inline static void from_zmq_message(const ofxZeroMQ::Message &m,
                                            ofxZeroMQ::Message &data)
        { from_zmq_message(m, static_cast<zmq::message_t &>(data)); };

#pragma mark std::string
        inline static void to_zmq_message(ofxZeroMQ::Message &m,
                                          const std::string &data)
        { m.rebuild(data.data(), data.length()); };

        inline static void from_zmq_message(const ofxZeroMQ::Message &m,
                                            std::string &data)
        { data = (char *)m.data(); };
        
#pragma mark standard layout type
        template <typename type>
        inline static auto to_zmq_message(ofxZeroMQ::Message &m,
                                          const type &data)
            -> ofxZeroMQ::detail::enable_if_standard_layout<type>
        { m.rebuild(&data, sizeof(type)); };

        template <typename type>
        inline static auto from_zmq_message(const ofxZeroMQ::Message &m,
                                            type &data)
            -> ofxZeroMQ::detail::enable_if_standard_layout<type>
        { std::memcpy((void *)&data, (const void *)m.data(), sizeof(type)); };
        
#pragma mark std::vector
        template <typename type>
        inline static auto to_zmq_message(ofxZeroMQ::Message &m,
                                          const std::vector<type> &data)
            -> ofxZeroMQ::detail::enable_if_standard_layout<type>
        { m.rebuild(data.data(), data.size() * sizeof(type)); };

        template <typename type>
        inline static auto from_zmq_message(const ofxZeroMQ::Message &m,
                                            std::vector<type> &data)
            -> ofxZeroMQ::detail::enable_if_standard_layout<type>
        {
            data.resize(m.size() / sizeof(type));
            std::memcpy(data.data(), m.data(), m.size());
        }
    
#pragma mark std::vector
        template <typename type>
        inline static void to_zmq_message(ofxZeroMQ::Message &m,
                                          const ofJson &data)
        { to_zmq_message(m, data.dump()); };

        template <typename type>
        inline static void from_zmq_message(const ofxZeroMQ::Message &m,
                                            ofJson &data)
        {
            std::string json_str;
            from_zmq_message(m, json_str);
            data = ofJson::parse(json_str);
        }
    }; // detail
}; // ofxZeroMQ

#include "detail/ofxZeroMQConvertFunctions.h"

#pragma mark - to/from_zmq_message

namespace ofxZeroMQ {
    namespace detail {
        template <typename type>
        struct constexpr_inline_variable {
            static constexpr const type value{};
        };
        template <typename type>
        constexpr const type constexpr_inline_variable<type>::value;
        
        struct to_zmq_message_fun {
            template<typename value_type>
            auto operator()(Message &m, value_type &&v) const noexcept(noexcept(to_zmq_message(m, std::forward<value_type>(v))))
            -> decltype(to_zmq_message(m, std::forward<value_type>(v)), void())
            { return to_zmq_message(m, std::forward<value_type>(v)); };
        };
        
        struct from_zmq_message_fun {
            template<typename value_type>
            auto operator()(const Message &m, value_type &v) const
                noexcept(noexcept(from_zmq_message(m, v)))
            -> decltype(from_zmq_message(m, v), void())
            { return from_zmq_message(m, v); }
        };
    };
    
    namespace {
        constexpr const auto &to_zmq_message = detail::constexpr_inline_variable<detail::to_zmq_message_fun>::value;
        constexpr const auto &from_zmq_message = detail::constexpr_inline_variable<detail::from_zmq_message_fun>::value;
    };
};

#pragma mark - adl converter

namespace ofxZeroMQ {
    template <typename type, typename sfinae>
    struct adl_converter {
        template <typename value_type>
        static auto from_zmq_message(const Message &m, value_type &val)
            noexcept(
                noexcept(::ofxZeroMQ::from_zmq_message(m, val))
            )
        -> decltype(::ofxZeroMQ::from_zmq_message(m, val), void())
        { ::ofxZeroMQ::from_zmq_message(m, val); };

        template <typename value_type>
        static auto to_zmq_message(Message &m, value_type &&val)
            noexcept(
                noexcept(::ofxZeroMQ::to_zmq_message(m, std::forward<value_type>(val)))
            )
        -> decltype(::ofxZeroMQ::to_zmq_message(m, std::forward<value_type>(val)), void())
        { ::ofxZeroMQ::to_zmq_message(m, std::forward<value_type>(val)); };
    };
};

#pragma mark - Message definition

namespace ofxZeroMQ {
    template <typename type>
    void Message::from(type &&v)
    { adl_converter<type>::to_zmq_message(*this, std::forward<type>(v)); };
    
    template <typename type>
    void Message::to(type &v) const
    { adl_converter<type>::from_zmq_message(*this, v); };
};

#pragma mark - MultipartMessage

namespace ofxZeroMQ {
    struct MultipartMessage : zmq::multipart_t {
        struct ArgumentConverter {
            ArgumentConverter(const MultipartMessage *message, std::size_t index)
            : message{message}
            , index{index}
            {};
            
            inline ArgumentConverter(const ArgumentConverter &) noexcept = default;
            inline ArgumentConverter(ArgumentConverter &&) noexcept = default;
            
            inline ArgumentConverter &operator=(const ArgumentConverter &) noexcept = default;
            inline ArgumentConverter &operator=(ArgumentConverter &&) noexcept = default;
            
            template <typename type>
            inline void to(type &data) const {
                if(message->size() <= index) {
                    ofLogWarning("ofxZeroMQMultipartMessage::operator[]") << "index out of bound.";
                    return;
                }
                adl_converter<type>::from_zmq_message(message->at(index), data);
            };
            
            template <typename type>
            inline type get() const {
                type data;
                to(data);
                return data;
            };

            template <typename type>
            inline operator type() const
            { return get<type>(); };
            
        private:
            const MultipartMessage *message;
            std::size_t index;
        };

        template <typename ... types>
        MultipartMessage(types && ... data)
        { addArguments(std::forward<types>(data) ...); };
        
        template <typename type>
        void addArgument(type &&data) {
            add(std::move(Message{std::forward<type>(data)}));
        };
        
        template <typename ... types>
        void addArguments(types && ... data) {
            Message messages[] = { convert(std::forward<types>(data)) ... };
            for(auto &&m : messages) add(std::move(m));
        }
        
        ArgumentConverter operator[](std::size_t index) const noexcept
        { return { this, index }; };
        
        template <typename ... types>
        bool convertTo(types & ... data) const
        { return convertTo(0, data ...); };
        
        template <typename ... types>
        bool rangedConvertTo(std::size_t from, std::size_t to, types & ... data) const
        {
            if(to < from) {
                ofLogWarning("ofxZeroMQMultipartMessage") << "range `to`(" << to << ") is smaller than `from` (" << from << ")";
            } else if(sizeof...(data) != to - from) {
                ofLogWarning("ofxZeroMQMultipartMessage") << "num arguments doesn't match to range. given " << sizeof...(data) << " arguments and range is [" << from << ", " << to << ") i.e. " << (to - from) << ".";
            }
            return rangedConvertTo(from, from, to, data ...);
        };
        
    protected:
        template <typename type>
        Message convert(type &&data) {
            Message m;
            to_zmq_message(m, std::forward<type>(data));
            return m;
        }
        
        template <typename type>
        bool convertTo(std::size_t n, type &data) const {
            if(n < size()) {
                adl_converter<type>::from_zmq_message(at(n), data);
                return true;
            } else {
                ofLogWarning("ofxZeroMQMultipartMessage") << "arguments num is larger than received message";
                return false;
            }
        }
        
        template <typename type, typename ... types>
        bool convertTo(std::size_t n, type &data, types & ... others) const {
            if(n < size()) {
                adl_converter<type>::from_zmq_message(at(n), data);
                if(n + 1 < size()) {
                    return convertTo(n + 1, others ...);
                } else {
                    ofLogWarning("ofxZeroMQMultipartMessage") << "arguments num is larger than received message";
                    return false;
                }
            } else {
                ofLogWarning("ofxZeroMQMultipartMessage") << "arguments num is larger than received message";
                return false;
            }
        }
        
        template <typename type>
        bool rangedConvertTo(std::size_t n,
                             std::size_t from,
                             std::size_t to,
                             type &data) const
        {
            if(to == n) return;
            if(n < size()) {
                adl_converter<type>::from_zmq_message(at(n), data);
                return true;
            } else {
                ofLogWarning("ofxZeroMQMultipartMessage") << "arguments num is larger than received message";
                return false;
            }
        }
        
        template <typename type, typename ... types>
        bool rangedConvertTo(std::size_t n,
                             std::size_t from,
                             std::size_t to,
                             type &data,
                             types & ... others) const
        {
            if(to == n) return;
            if(n < size()) {
                adl_converter<type>::from_zmq_message(at(n), data);
                if(n + 1 < size()) {
                    return convertTo(n + 1, others ...);
                } else {
                    ofLogWarning("ofxZeroMQMultipartMessage") << "arguments num is larger than received message";
                    return false;
                }
            } else {
                ofLogWarning("ofxZeroMQMultipartMessage") << "arguments num is larger than received message";
                return false;
            }
        }
    };
};

#pragma mark - Socket and other implementations

namespace ofxZeroMQ {
    struct Socket {
        virtual ~Socket() {
            socket.close();
        };
        
        void setIdentity(const std::string &data)
        { socket.setsockopt(ZMQ_IDENTITY, data.data(), data.size()); };

        std::string getIdentity() {
            char buf[256];
            std::size_t size{0};
            socket.getsockopt(ZMQ_IDENTITY, buf, &size);
            return { buf, buf + size };
        }

        bool isConnected() const
        { return socket.connected(); };

        OF_DEPRECATED_MSG("use setReceiveHighWaterMark or setSendHighWaterMark explicitly. this method set both.",
                          void setHighWaterMark(std::int32_t maxQueueSize))
        {
            setReceiveHighWaterMark(maxQueueSize);
            setSendHighWaterMark(maxQueueSize);
        }
        void setSendHighWaterMark(std::int32_t maxQueueSize) {
            socket.setsockopt(ZMQ_SNDHWM, &maxQueueSize, sizeof(std::int32_t));
        }
        void setReceiveHighWaterMark(std::int32_t maxQueueSize) {
            socket.setsockopt(ZMQ_RCVHWM, &maxQueueSize, sizeof(std::int32_t));
        }

        OF_DEPRECATED_MSG("use getSendHighWaterMark or getReceiveHighWaterMark explicitly. getHighWaterMark returns `getSendHighWaterMark()`",
                          std::int32_t getHighWaterMark())
        { return getSendHighWaterMark(); };
        
        std::int32_t getSendHighWaterMark() {
            std::int32_t v;
            std::size_t size = sizeof(v);
            socket.getsockopt(ZMQ_SNDHWM, &v, &size);
            return v;
        }
        
        std::int32_t getReceiveHighWaterMark() {
            std::int32_t v;
            std::size_t size = sizeof(v);
            socket.getsockopt(ZMQ_RCVHWM, &v, &size);
            return v;
        }
        
        zmq::socket_t &getRawSocket()
        { return socket; };
        const zmq::socket_t &getRawSocket() const
        { return socket; };
    protected:
        Socket(int type)
        : socket(get_context(), type)
        {
            item.socket = socket;
            item.fd = 0;
            item.events = ZMQ_POLLIN;
            item.revents = 0;
        };
        
        void connect(const std::string &address)
        { socket.connect(address.c_str()); };
        void bind(const std::string &address)
        { socket.bind(address.c_str()); };
        void disconnect(const std::string &address)
        { socket.disconnect(address.c_str()); };
        void unbind(const std::string &address)
        { socket.unbind(address.c_str()); };
        
        zmq::send_result_t send(const void *data,
                                std::size_t length,
                                bool nonblocking = true,
                                bool more = false)
        {
            return socket.send(std::move(Message{data, length}),
                               zmq::send_flags(SendFlag{nonblocking, more}));
        }
        
        template <
            typename type,
            typename = typename std::enable_if<!std::is_pointer<type>::value>::type
        >
        zmq::send_result_t send(const type &data,
                                bool nonblocking = true,
                                bool more = false)
        {
            return socket.send(std::move(Message{data}),
                               zmq::send_flags(SendFlag{nonblocking, more}));
        };
                
        zmq::send_result_t send(MultipartMessage &mess,
                                bool nonblocking = true,
                                bool more = false)
        {
            return mess.send(socket, SendFlag{nonblocking, more});
        };
        
        template <typename ... types>
        auto sendMultipart(types && ... data)
            -> typename std::enable_if<
                !detail::last_arg_is_SendFlag<types ...>::value,
                zmq::send_result_t
            >::type
        {
            return MultipartMessage{std::forward<types>(data) ...}.send(socket, SendFlag{});
        }

        template <typename ... types>
        auto sendMultipart(types && ... data)
            -> typename std::enable_if<
                detail::last_arg_is_SendFlag<types ...>::value,
                zmq::send_result_t
            >::type
        {
            auto flag = std::get<sizeof...(types) - 1>(std::forward_as_tuple(std::forward<types>(data) ...));
            return MultipartMessage{std::forward<types>(data) ...}.send(socket, flag);
        }
        
        template <typename type>
        bool receive(type &data, ReceiveFlag flags = ReceiveFlag{})
        {
            Message m;
            auto &&result = receive(m);
            if(result.first.has_value()) {
                adl_converter<type>::from_zmq_message(m, data);
            }
            return result.second;
        }
        
        bool receive(MultipartMessage &message,
                     ReceiveFlag flags = ReceiveFlag{})
        { return message.recv(socket, flags); };
        
        bool receiveMultipart(MultipartMessage &message,
                              ReceiveFlag flags = ReceiveFlag{})
        { return message.recv(socket, flags); };

        template <typename ... types>
        auto receiveMultipart(types & ... data)
            -> typename std::enable_if<
                detail::last_arg_is_ReceiveFlag<types ...>::value,
                bool
            >::type
        {
            auto flag = std::get<sizeof...(types) - 1>(std::forward_as_tuple(std::forward<types>(data) ...));
            MultipartMessage message;
            receiveMultipart(message);
            return message.rangedConvertTo(0, sizeof...(types) - 1, data ...);
        }

        template <typename ... types>
        auto receiveMultipart(types & ... others)
            -> typename std::enable_if<
                !detail::last_arg_is_ReceiveFlag<types ...>::value,
                bool
            >::type
        {
            MultipartMessage message;
            receiveMultipart(message);
            return message.convertTo(others ...);
        }

        bool hasWaitingMessage(long timeout_millis = 0)
        { return 0 < zmq::poll(&item, 1, timeout_millis * 1000); }
        
        // return true if has more flag
        template <typename data_type>
        bool getNextMessage(data_type &data) {
            if(item.revents & ZMQ_POLLIN) return receive(data);
            return false;
        }
        
        template <typename ... data_types>
        std::size_t getNextMessages(data_types & ... data)
        {
            if(item.revents & ZMQ_POLLIN) return receiveMultipart(data ...);
            return 0;
        }
        
        zmq::socket_t socket;
        zmq::pollitem_t item;
    protected:
        std::pair<zmq::recv_result_t, bool> receive(Message &m) {
            std::int32_t more;
            std::size_t more_size{sizeof(more)};
            
            zmq::recv_flags flags;
            
            auto &&res = socket.recv(m, flags);
            return {res, m.more()};
        }
        
    private:
        static zmq::context_t &get_context() {
            static zmq::context_t context{4};
            return context;
        }
    };
    
#pragma mark -
    struct Publisher : Socket {
        Publisher()
        : Socket(ZMQ_PUB)
        {};
        
        using Socket::bind;
        using Socket::unbind;
        
        // for xpub-xsub pattern
        using Socket::connect;
        using Socket::disconnect;
        
        using Socket::send;
        using Socket::sendMultipart;
    };
    
#pragma mark -
    struct Subscriber : Socket {
        Subscriber()
        : Socket(ZMQ_SUB)
        {};
        
        using Socket::disconnect;
        
        using Socket::receive;
        using Socket::receiveMultipart;
        
        using Socket::hasWaitingMessage;
        using Socket::getNextMessage;
        using Socket::getNextMessages;

        void connect(const std::string &address) {
            if(filters.empty()) addFilter("");
            Socket::connect(address);
        }
        void addFilter(const std::string &filter) {
            filters.insert(filter);
            socket.setsockopt(ZMQ_SUBSCRIBE, filter.data(), filter.size());
        }
        // return false if give filter string is not subscribed
        bool removeFilter(const std::string &filter) {
            if(0 < filters.erase(filter)) {
                socket.setsockopt(ZMQ_UNSUBSCRIBE, filter.data(), filter.size());
                return true;
            }
            return false;
        }
        void removeAllFilters() {
            for(const auto &v : filters) socket.setsockopt(ZMQ_UNSUBSCRIBE, v.data(), v.size());
            filters.clear();
        }
    private:
        std::set<std::string> filters;
    };
    
#pragma mark -
    struct Request : Socket {
        Request()
        : Socket(ZMQ_REQ)
        {};
        
        using Socket::connect;
        using Socket::disconnect;
        
        using Socket::send;
        using Socket::sendMultipart;
        
        using Socket::receive;
        using Socket::receiveMultipart;
        
        using Socket::hasWaitingMessage;
        using Socket::getNextMessage;
        using Socket::getNextMessages;
    };
    
#pragma mark -
    struct Reply : Socket {
        Reply()
        : Socket(ZMQ_REP)
        {};
        
        using Socket::bind;
        using Socket::unbind;
        
        using Socket::send;
        using Socket::sendMultipart;
        
        using Socket::receive;
        using Socket::receiveMultipart;

        using Socket::hasWaitingMessage;
        using Socket::getNextMessage;
        using Socket::getNextMessages;
    };
    
#pragma mark -
    struct Push : Socket {
        Push()
        : Socket(ZMQ_PUSH)
        {};
        
        using Socket::bind;
        using Socket::unbind;
        
        using Socket::connect;
        using Socket::disconnect;

        using Socket::send;
        using Socket::sendMultipart;
        
        using Socket::receive;
        using Socket::receiveMultipart;

        using Socket::hasWaitingMessage;
        using Socket::getNextMessage;
        using Socket::getNextMessages;
    };
    
#pragma mark -
    struct Pull : Socket {
        Pull()
        : Socket(ZMQ_PULL)
        {};
        
        using Socket::bind;
        using Socket::unbind;
        
        using Socket::connect;
        using Socket::disconnect;

        using Socket::send;
        using Socket::sendMultipart;
        
        using Socket::receive;
        using Socket::receiveMultipart;

        using Socket::hasWaitingMessage;
        using Socket::getNextMessage;
        using Socket::getNextMessages;
    };

#pragma mark -
    struct Pair : Socket {
        Pair()
        : Socket(ZMQ_PAIR)
        {};
        
        using Socket::connect;
        using Socket::disconnect;
        
        using Socket::bind;
        using Socket::unbind;
        
        using Socket::send;
        using Socket::sendMultipart;
        
        using Socket::receive;
        using Socket::receiveMultipart;

        using Socket::hasWaitingMessage;
        using Socket::getNextMessage;
        using Socket::getNextMessages;
    };
    
#pragma mark -
    struct Router : Socket {
        Router()
        : Socket(ZMQ_ROUTER)
        {};
        
        using Socket::bind;
        using Socket::unbind;
        
        using Socket::sendMultipart;
        using Socket::receiveMultipart;
        
        using Socket::hasWaitingMessage;
    };
    
#pragma mark -
    struct Dealer : Socket {
        Dealer()
        : Socket(ZMQ_DEALER)
        {};
        
        using Socket::bind;
        using Socket::unbind;
        
        using Socket::sendMultipart;
        using Socket::receiveMultipart;

        using Socket::hasWaitingMessage;
    };
    
#pragma mark -
    struct XPublisher : Socket {
        XPublisher()
        : Socket(ZMQ_XPUB)
        {};
        
        using Socket::bind;
        using Socket::unbind;

        using Socket::send;
        using Socket::sendMultipart;
    };
    
#pragma mark -
    struct XSubscriber : Socket {
        XSubscriber()
        : Socket(ZMQ_XSUB)
        {};
        
        using Socket::bind;
        using Socket::unbind;

        using Socket::receive;
        using Socket::receiveMultipart;
        
        using Socket::hasWaitingMessage;
        using Socket::getNextMessage;
        using Socket::getNextMessages;
    };
    
#pragma mark -
    struct Broker {
        Broker() {};
        ~Broker() {
            is_running = false;
            while(!is_finish) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        void setup(const std::string &router_address,
                   const std::string &dealer_address)
        {
            router.bind(router_address);
            dealer.bind(dealer_address);
            is_finish = false;
            is_running = true;
            std::thread([this] {
                while(is_running) {
                    MultipartMessage m;
                    while(router.hasWaitingMessage()) {
                        router.receiveMultipart(m);
                        dealer.sendMultipart(std::move(m));
                    }
                    while(dealer.hasWaitingMessage()) {
                        dealer.receiveMultipart(m);
                        router.sendMultipart(std::move(m));
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                is_finish = true;
            }).detach();
        }
        
        Router router;
        Dealer dealer;
        std::atomic_bool is_running;
        std::atomic_bool is_finish;
    };

#pragma mark -
    struct XPubSubProxy {
        XPubSubProxy()
        {};
        ~XPubSubProxy() {
            is_running = false;
            while(!is_finish) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        void setup(const std::string &pub_address,
                   const std::string &sub_address)
        {
            pub.bind(pub_address);
            sub.bind(sub_address);
        
            std::thread([this] {
                while(is_running) {
                    MultipartMessage m;
                    while(sub.hasWaitingMessage()) {
                        sub.receiveMultipart(m);
                        pub.sendMultipart(std::move(m));
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                is_finish = true;
            }).detach();
        }
        
        XPublisher &getXPublisher()
        { return pub; };
        const XPublisher &getXPublisher() const
        { return pub; };
        
        XSubscriber &getXSubscriber()
        { return sub; };
        const XSubscriber &getXSubscriber() const
        { return sub; };

    protected:
        XPublisher pub;
        XSubscriber sub;
        std::atomic_bool is_running;
        std::atomic_bool is_finish;
    };
};

using ofxZeroMQMessage = ofxZeroMQ::Message;
using ofxZeroMQMultipartMessage = ofxZeroMQ::MultipartMessage;
using ofxZeroMQSocket = ofxZeroMQ::Socket;

using ofxZeroMQPublisher = ofxZeroMQ::Publisher;
using ofxZeroMQSubscriber = ofxZeroMQ::Subscriber;

using ofxZeroMQRequest = ofxZeroMQ::Request;
using ofxZeroMQReply = ofxZeroMQ::Reply;

using ofxZeroMQPair = ofxZeroMQ::Pair;

using ofxZeroMQRouter = ofxZeroMQ::Router;
using ofxZeroMQDealer = ofxZeroMQ::Dealer;
using ofxZeroMQBroker = ofxZeroMQ::Broker;

using ofxZeroMQXPublisher = ofxZeroMQ::XPublisher;
using ofxZeroMQXSubscriber = ofxZeroMQ::XSubscriber;
using ofxZeroMQXPubSubProxy = ofxZeroMQ::XPubSubProxy;

#endif /* ofxZeroMQ_h */
