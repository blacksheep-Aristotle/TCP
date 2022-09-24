#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.

/*
CLOSED状态：初始状态，表示TCP连接是"关闭的"或者"未打开的"

LISTEN状态：表示服务端的某个端口正处于监听状态，正在等待客户端连接的到来

SYN_SENT状态：当客户端发送SYN请求建立连接之后，客户端处于SYN_SENT状态，等待服务器发送SYN+ACK

SYN_RCVD状态：当服务器收到来自客户端的连接请求SYN之后，服务器处于SYN_RCVD状态，在接收到SYN请求之后会向客户端回复一个SYN+ACK的确认报文

ESTABLISED状态：当客户端回复服务器一个ACK和服务器收到该ACK（TCP最后一次握手）之后，服务器和客户端都处于该状态，表示TCP连接已经成功建立

FIN_WAIT_1状态：当数据传输期间当客户端想断开连接，向服务器发送了一个FIN之后，客户端处于该状态

FIN_WAIT_2状态：当客户端收到服务器发送的连接断开确认ACK之后，客户端处于该状态

CLOSE_WAIT状态：当服务器发送连接断开确认ACK之后但是还没有发送自己的FIN之前的这段时间，服务器处于该状态

TIME_WAIT状态：当客户端收到了服务器发送的FIN并且发送了自己的ACK之后，客户端处于该状态

LAST_ACK状态：表示被动关闭的一方（比如服务器）在发送FIN之后，等待对方的ACK报文时，就处于该状态

CLOSING状态：连接断开期间，一般是客户端发送一个FIN，然后服务器回复一个ACK，然后服务器发送完数据后再回复一个FIN，当客户端和服务器同时接受到FIN时，客户端和服务器处于CLOSING状态，也就是此时双方都正在关闭同一个连接
*/

class TCPSender {
  enum Sen_statue{
    LISTEN =1,    //初始状态
    SYN_SEND=2,  //客户端发送了syn，收到ACK并回应一个ACK时进入EST
    //SYN_RCVD=3,  //接受端收到SYN，收到ACK时进入EST
    EST =4,      //建立连接（客户端发出第一个ACK，服务端收到第一个ACK）
    FIN_WAIT_1=5, //客户端发送FIn 
    FIN_WAIT_2=6, //客户端收到对FIN的ACK
    //CLOSE_WAIT= 7, //客户端结束连接，但服务端还未结束
    TIME_WAIT =8,  //客户端收到FIN且发送了ACK
    //LAST_ACK=9,   //被动关闭的那一方发送FIN，等待ACK时
    CLOSING=10,
    

  }

  private:
    Sen_statue _statue;
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;
    uint32_t _timeout; //超时时间
    uint32_t _timecount; //当time超过timeout时重发数据
    size_t _outgoing_bytes{0};  //已发出未确认的字节数
    size_t _retrycnt; //重传次数
    size_t _win;  //对方发出的窗口
    //! outbound queue of segments that the TCPSender wants sent
    //要发送的数据存在这个队列，调用发送函数时才用
    std::queue<TCPSegment> _segments_out{};
    //记录发出的包
    std::map<size_t, TCPSegment> _outgoing_map{};
    //! retransmission timer for the connection
    //用于还原timeout
    unsigned int _initial_retransmission_timeout;

    //! outgoing stream of bytes that have not yet been sent
    //还没有发送的字节流
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    //要发送的下一个字节流
    uint64_t _next_seqno{0};
    

  public:
    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();
    
    void send_segment();
    //! \brief Notifies the TCPSender of the passage of time
    //该函数将会被调用以指示经过的时间长度。经过上次tick的时间。
    //追踪的是发送者距离上次接受到ack经过的时间，而不是每个发送的包的时间
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
