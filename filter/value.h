/* -*- Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ifndef _VALUE_H_
#define _VALUE_H_

#include <cstdlib> // for memcpy
#include <cstring> // for memcpy
#include "printutil.h"

namespace hashfilter {
    class PrintUtil;

	class Value
	{
    private:
		char* data_;
		size_t size_;
        
        void init_copy(const char* p, const size_t size) {
            data_ = new char[size];
            size_ = size; 
            std::memcpy(const_cast<char*>(data_), p, size);
        }

    public:
		explicit Value(): data_(NULL), size_(0) { }

        Value(const char* p, const size_t size) {
            init_copy(p, size);
        }

        Value(const std::string&s) {
            init_copy(s.data(), s.size());
        }

		Value(const Value& rhs) {
            init_copy(rhs.data_, rhs.size_);
        }

		~Value() { delete [] data_; }

        inline bool operator==(const Value& rhs) const {
            return size_ == rhs.size_ && std::memcmp(data_, rhs.data_, size_) == 0;
        }

        inline bool operator!=(const Value& rhs) const {
            return !(*this == rhs);
        }

        inline Value& operator=(const Value& rhs) {
            if (size_)
                delete [] data_;
            init_copy(rhs.data_, rhs.size_);
            return *this;
        }

        inline int compare(const Value& rhs) const {
            size_t llen = size();
            size_t rlen = rhs.size();
            size_t mlen = llen <= rlen ? llen : rlen;
            int cmp = std::memcmp(data(), rhs.data(), mlen);
            if (cmp != 0)
                return cmp;
            else
                return llen - rlen;
        }

		const char* data() const { return data_; }

		const size_t& size() const { return size_; }

        std::string str() const { return std::string(data_, size_); }

        std::string hexstr() const { 
            return PrintUtil::bytes_to_hex(data_, size_); 
        }
        

		template <typename T>
		const T& as() const { return *reinterpret_cast<const T*>(data_); }

    };


} // namespace hashfilter

#endif  // #ifndef _VALUE_H_
