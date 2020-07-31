/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "util_ffmpeg.hpp"
#include <fsys/filesystem.h>

using namespace media;

AVFileIO::AVFileIO(const std::shared_ptr<ICustomFile> &fileInterface)
	: m_fileInterface{fileInterface}
{}
bool AVFileIO::open(const std::string &fileName)
{
	m_fileName = fileName;
	return m_fileInterface->open(fileName);
}
ssize_t AVFileIO::write(const uint8_t *data, size_t size)
{
	auto res = m_fileInterface->write(data,size);
	return res.has_value() ? *res : -1;
}
ssize_t AVFileIO::read(uint8_t *data, size_t size)
{
	auto res = m_fileInterface->read(data,size);
	return res.has_value() ? *res : -1;
}
int64_t AVFileIO::seek(int64_t offset, int whence)
{
	auto res = m_fileInterface->seek(offset,whence);
	return res.has_value() ? *res : -1;
}
int AVFileIO::seekable() const {return AVIO_SEEKABLE_NORMAL;}
const char *AVFileIO::name() const {return m_fileName.c_str();}

/////////////

AVFileIOFSys::AVFileIOFSys(VFilePtr f)
	: m_file{f}
{}
bool AVFileIOFSys::open(const std::string &fileName)
{
	return true;
}
ssize_t AVFileIOFSys::write(const uint8_t *data, size_t size)
{
	return 0;
}
ssize_t AVFileIOFSys::read(uint8_t *data, size_t size)
{
	return m_file->Read(data,size);
}
int64_t AVFileIOFSys::seek(int64_t offset, int whence)
{
	switch(whence)
	{
	case AVSEEK_SIZE:
		return m_file->GetSize();
	}
	m_file->Seek(offset,whence);
	return m_file->Tell();
}
int AVFileIOFSys::seekable() const
{
	return true;
}
const char *AVFileIOFSys::name() const
{
	return "";
}

/////////////

void media::check_error(std::error_code errCode)
{
	if(errCode)
		throw RuntimeError{"AVCPP Error: " +errCode.message()};
}
