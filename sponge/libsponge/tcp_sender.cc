#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity),_timecount(0),_timeout(retx_timeout),_statue(Sen_statue::LISTEN),_win(0) {
    
    }
//有多少发出还未收到ack的字节(包不包括SYN和FIN？）
uint64_t TCPSender::bytes_in_flight() const {
    return _fly;
}
//不用考虑第二次才能发送数据，因为第一次握手的窗口大小=0

/*
当循环填充发送窗口时，若发送窗口大小足够但本地没有数据包需要发送，则必须停止发送。

若当前 Segment 是 FIN 包，则在发送完该包后，立即停止填充发送窗口。
*/
/*
：TCPSender 从 ByteStream 中读取数据，并以 TCPSegement 的形式发送，尽可能地填充接收者的窗口。但每个TCP段的大小不得超过 TCPConfig::MAX PAYLOAD SIZE。

若接收方的 Windows size 为 0，则发送方将按照接收方 window size 为 1 的情况进行处理，持续发包。
*/
void size_t curr_window_size = _last_window_size ? _last_window_size : 1;
    // 循环填充窗口
    while (curr_window_size > _outgoing_bytes) {
        // 尝试构造单个数据包
        // 如果此时尚未发送 SYN 数据包，则立即发送
        TCPSegment segment;
        if (!_set_syn_flag) {
            segment.header().syn = true;
            _set_syn_flag = true;
        }
        // 设置 seqno
        segment.header().seqno = next_seqno();

        // 装入 payload.
        const size_t payload_size =
            min(TCPConfig::MAX_PAYLOAD_SIZE, curr_window_size - _outgoing_bytes - segment.header().syn);
        string payload = _stream.read(payload_size);

        /**
         * 读取好后，如果满足以下条件，则增加 FIN
         *  1. 从来没发送过 FIN
         *  2. 输入字节流处于 EOF
         *  3. window 减去 payload 大小后，仍然可以存放下 FIN
         */
        if (!_set_fin_flag && _stream.eof() && payload.size() + _outgoing_bytes < curr_window_size)
            _set_fin_flag = segment.header().fin = true;

        segment.payload() = Buffer(move(payload));

        // 如果没有任何数据，则停止数据包的发送
        if (segment.length_in_sequence_space() == 0)
            break;

        // 如果没有正在等待的数据包，则重设更新时间
        if (_outgoing_map.empty()) {
            _timeout = _initial_retransmission_timeout;
            _timecount = 0;
        }

        // 发送
        _segments_out.push(segment);

        // 追踪这些数据包
        _outgoing_bytes += segment.length_in_sequence_space();
        _outgoing_map.insert(make_pair(_next_seqno, segment));
        // 更新待发送 abs seqno
        _next_seqno += segment.length_in_sequence_space();

        // 如果设置了 fin，则直接退出填充 window 的操作
        if (segment.header().fin)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _win=window_size; 
    for(auto seq:_outgoing_map){
        if(seq->first > ackno){
            brek;
        }
        _outgoing_map.pop();
        _consecutive_retransmissions_count=0;
        _timeout=0;
    } 
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
//超时只发送最早的那个seq，并且在win=0时不增加连续重传定时器！！！！
void TCPSender::tick(const size_t ms_since_last_tick) { 
    size_t abs_seqno = unwrap(ackno, _isn, _next_seqno);
    // 如果传入的 ack 是不可靠的，则直接丢弃
    if (abs_seqno > _next_seqno)
        return;
    // 遍历数据结构，将已经接收到的数据包丢弃
    for (auto iter = _outgoing_map.begin(); iter != _outgoing_map.end();) {
        // 如果一个发送的数据包已经被成功接收
        const TCPSegment &seg = iter->second;
        if (iter->first + seg.length_in_sequence_space() <= abs_seqno) {
            _outgoing_bytes -= seg.length_in_sequence_space();
            iter = _outgoing_map.erase(iter);

            // 如果有新的数据包被成功接收，则清空超时时间
            _timeout = _initial_retransmission_timeout;
            _timecount = 0;
        }
        // 如果当前遍历到的数据包还没被接收，则说明后面的数据包均未被接收，因此直接返回
        else
            break;
    }
    _consecutive_retransmissions_count = 0;
    // 填充后面的数据
    _last_window_size = window_size;
    fill_window();

}

unsigned int TCPSender::consecutive_retransmissions() const { return {}; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}

}
