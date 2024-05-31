#pragma once

#include "Common/File/VFS/VFS.h"
#include "Common/File/FileUtil.h"
#include "Common/File/Path.h"
#include <memory>

class DirectoryReader : public VFSBackend {
public:
	explicit DirectoryReader(const Path &path);
	// use delete[] on the returned value.
	uint8_t *ReadFile(const char *path, size_t *size) override;

	std::unique_ptr<VFSFileReference> GetFile(const char *path) override;
	bool GetFileInfo(VFSFileReference *vfsReference, File::FileInfo *fileInfo) override;
	void ReleaseFile(std::unique_ptr<VFSFileReference> vfsReference) override;

	std::unique_ptr<VFSOpenFile> OpenFileForRead(VFSFileReference *vfsReference, size_t *size) override;
	void Rewind(VFSOpenFile *vfsOpenFile) override;
	size_t Read(VFSOpenFile *vfsOpenFile, void *buffer, size_t length) override;
	void CloseFile(std::unique_ptr<VFSOpenFile> vfsOpenFile) override;

	bool GetFileListing(const char *path, std::vector<File::FileInfo> *listing, const char *filter) override;
	bool GetFileInfo(const char *path, File::FileInfo *info) override;
	bool Exists(const char *path) override;
	std::string toString() const override {
		return path_.ToString();
	}

private:
	Path path_;
};
