//
//  IFileSystem.hpp
//  Ultralight
//
//  Created by miyako on 2019/09/27.
//  Copyright © 2019 miyako. All rights reserved.
//

#ifndef IFileSystem_hpp
#define IFileSystem_hpp

#include "4DPluginAPI.h"

#include <iostream>
#include <string>
#include <memory>
#include <map>
#include <vector>

#include <Ultralight/Ultralight.h>

#if VERSIONMAC
#import <Foundation/Foundation.h>
#import <CoreServices/CoreServices.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

using namespace ultralight;

class IFileSystem : public FileSystem {
    
public:
    
    IFileSystem();
    ~IFileSystem();
    
    bool FileExists(const String16& path);
    bool DeleteFile_(const String16& path);
    bool DeleteEmptyDirectory(const String16& path);
    bool MoveFile_(const String16& old_path, const String16& new_path);
    bool GetFileSize(const String16& path, int64_t& result);
    bool GetFileSize(FileHandle handle, int64_t& result);
    bool GetFileMimeType(const String16& path, String16& result);
    bool GetFileModificationTime(const String16& path, time_t& result);
    bool GetFileCreationTime(const String16& path, time_t& result);
    MetadataType GetMetadataType(const String16& path);
    String16 GetPathByAppendingComponent(const String16& path, const String16& component);
    bool CreateDirectory_(const String16& path);
    String16 GetHomeDirectory();
    String16 GetFilenameFromPath(const String16& path);
    String16 GetDirectoryNameFromPath(const String16& path);
    bool GetVolumeFreeSpace(const String16& path, uint64_t& result);
    int32_t GetVolumeId(const String16& path);
    Ref<String16Vector> ListDirectory(const String16& path, const String16& filter);
    String16 OpenTemporaryFile(const String16& prefix, FileHandle& handle);
    FileHandle OpenFile(const String16& path, bool open_for_writing);
    void CloseFile(FileHandle& handle);
    int64_t SeekFile(FileHandle handle, int64_t offset, FileSeekOrigin origin);
    bool TruncateFile(FileHandle handle, int64_t offset);
    int64_t WriteToFile(FileHandle handle, const char* data, int64_t length);
    int64_t ReadFromFile(FileHandle handle, char* data, int64_t length);
    bool CopyFile_(const String16& source_path, const String16& destination_path);
    
};

#endif /* IFileSystem_hpp */
