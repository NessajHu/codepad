#include "../filesystem.h"

/// \file
/// Filesystem implementation for the windows platform.

#include "../windows.h"

namespace codepad::os {
	const file::native_handle_t file::empty_handle = INVALID_HANDLE_VALUE;

	/// Transforms the given \ref access_rights into flags used by \p CreateFile().
	inline DWORD _interpret_access_rights(access_rights acc) {
		return
			(test_bit_any(acc, access_rights::read) ? FILE_GENERIC_READ : 0) |
			(test_bit_any(acc, access_rights::write) ? FILE_GENERIC_WRITE : 0);
	}
	/// Transforms the given \ref open_mode into flags used by \p CreateFile().
	inline DWORD _interpret_open_mode(open_mode mode) {
		switch (mode) {
		case open_mode::create:
			return CREATE_NEW;
		case open_mode::create_or_truncate:
			return CREATE_ALWAYS;
		case open_mode::open:
			return OPEN_EXISTING;
		case open_mode::open_and_truncate:
			return TRUNCATE_EXISTING;
		case open_mode::open_or_create:
			return OPEN_ALWAYS;
		}
		assert_true_usage(false, "invalid open mode");
		return OPEN_EXISTING;
	}

	file::native_handle_t file::_open_impl(
		const std::filesystem::path &path, access_rights acc, open_mode mode
	) {
		native_handle_t res = CreateFile(
			path.c_str(), _interpret_access_rights(acc),
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
			_interpret_open_mode(mode), FILE_ATTRIBUTE_NORMAL, nullptr
		);
		if (res == INVALID_HANDLE_VALUE) {
			logger::get().log_warning(CP_HERE, "CreateFile failed with error code ", GetLastError());
		}
		return res;
	}
	void file::_close_impl() {
		winapi_check(CloseHandle(_handle));
	}
	file::size_type file::_get_size_impl() const {
		LARGE_INTEGER sz;
		winapi_check(GetFileSizeEx(_handle, &sz));
		return sz.QuadPart;
	}


	file_mapping::file_mapping(file_mapping &&rhs) : _ptr(rhs._ptr), _handle(rhs._handle) {
		rhs._ptr = nullptr;
		rhs._handle = nullptr;
	}
	file_mapping &file_mapping::operator=(file_mapping &&rhs) {
		std::swap(_ptr, rhs._ptr);
		std::swap(_handle, rhs._handle);
		return *this;
	}
	void file_mapping::_map_impl(const file &file, access_rights acc) {
		_handle = CreateFileMapping(
			file.get_native_handle(), nullptr,
			acc == access_rights::read ? PAGE_READONLY : PAGE_READWRITE,
			0, 0, nullptr
		);
		if (_handle != nullptr) {
			assert_true_usage(GetLastError() != ERROR_ALREADY_EXISTS, "cannot open multiple mappings to one file");
			_ptr = MapViewOfFile(_handle, acc == access_rights::read ? FILE_MAP_READ : FILE_MAP_WRITE, 0, 0, 0);
			if (!_ptr) {
				logger::get().log_warning(CP_HERE, "MapViewOfFile failed with error code ", GetLastError());
				winapi_check(CloseHandle(_handle));
			}
		} else {
			logger::get().log_warning(CP_HERE, "CreateFileMapping failed with error code ", GetLastError());
		}
	}
	void file_mapping::_unmap_impl() {
		winapi_check(UnmapViewOfFile(_ptr));
		winapi_check(CloseHandle(_handle));
		_ptr = nullptr;
		_handle = nullptr;
	}
	size_t file_mapping::get_mapped_size() const {
		if (valid()) {
			MEMORY_BASIC_INFORMATION info;
			winapi_check(VirtualQuery(_ptr, &info, sizeof(info)));
			return info.RegionSize;
		}
		return 0;
	}
}