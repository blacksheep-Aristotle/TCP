#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity):_next_assembled_idx(0) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    //acc 
    if(index<=_next_assembled_idx){
    	data=substr(_next_assembled_idx-index);
    	int len=data.size();
    	int wlen=_output.write(data);
    	if(len<wlen)){
		int tmpidx=_next_assembled_idx+wlen;
		_unassemble_strs[tmpidx]=data.substr(tmpidx);
	}
    }
    //not 
    if(m_size){
    	
    }
}

size_t StreamReassembler::unassembled_bytes() const { return {}; }

bool StreamReassembler::empty() const { return {}; }
