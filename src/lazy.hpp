//
// Created by dafitius on 02-10-2026.
//

#ifndef MULTIMEDIARETRIEVAL_LAZY_H
#define MULTIMEDIARETRIEVAL_LAZY_H
#include <optional>
#include <utility>

template <typename T>
class Lazy {
public:
	template <typename F>
	const T& getOrCreate(F&& f) const {
		if (!value_) {
			value_.emplace(std::forward<F>(f)());
		}
		return *value_;
	}

	template <typename F>
	const std::optional<T>& getOrTryCreate(F&& f) const {
		if (!value_) {
			value_ = std::forward<F>(f)();
		}
		return value_;
	}

	const T& get() const {
		return *value_;
	}

	void emplace(const T& value) {
		value_.emplace(value);
	}

	void reset() noexcept {
		value_.reset();
	}

	[[nodiscard]] bool hasValue() const noexcept {
		return value_.has_value();
	}

private:
	mutable std::optional<T> value_;
};


#endif // MULTIMEDIARETRIEVAL_LAZY_H
