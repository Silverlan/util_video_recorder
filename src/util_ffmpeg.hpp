/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __UTIL_FFMPEG_HPP__
#define __UTIL_FFMPEG_HPP__

#include <formatcontext.h>
#include "util_media.hpp"

class VFilePtrInternal;
using VFilePtr = std::shared_ptr<VFilePtrInternal>;
namespace media
{
	struct AVFileIO
		: public av::CustomIO
	{
		AVFileIO(const std::shared_ptr<ICustomFile> &fileInterface);
		bool open(const std::string &fileName);
		virtual ssize_t write(const uint8_t *data, size_t size) override;
		virtual ssize_t read(uint8_t *data, size_t size) override;
		virtual int64_t seek(int64_t offset, int whence) override;
		virtual int seekable() const override;
		virtual const char *name() const override;
	private:
		std::string m_fileName;
		std::shared_ptr<ICustomFile> m_fileInterface = nullptr;
	};

	struct AVFileIOFSys
		: public av::CustomIO
	{
		AVFileIOFSys(VFilePtr f);
		bool open(const std::string &fileName);
		virtual ssize_t write(const uint8_t *data, size_t size) override;
		virtual ssize_t read(uint8_t *data, size_t size) override;
		virtual int64_t seek(int64_t offset, int whence) override;
		virtual int seekable() const override;
		virtual const char *name() const override;
	private:
		VFilePtr m_file = nullptr;
	};

	void check_error(std::error_code errCode);
};

#endif
