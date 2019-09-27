//
//  IFileSystem.cpp
//  Ultralight
//
//  Created by miyako on 2019/09/27.
//  Copyright © 2019 miyako. All rights reserved.
//

#include "IFileSystem.hpp"

bool IFileSystem::FileExists(const String16& path) {
    
    return false;
}

bool IFileSystem::DeleteFile_(const String16& path) {
    
    return true;
}

bool IFileSystem::DeleteEmptyDirectory(const String16& path) {
    
    return true;
}

bool IFileSystem::MoveFile_(const String16& old_path, const String16& new_path) {
    
    return true;
}

bool IFileSystem::GetFileSize(const String16& path, int64_t& result) {
    
    return true;
}

bool IFileSystem::GetFileSize(FileHandle handle, int64_t& result) {
    
    return true;
}

bool IFileSystem::GetFileMimeType(const String16& path, String16& result) {
    
    return true;
}

bool IFileSystem::GetFileModificationTime(const String16& path, time_t& result) {
    
    return true;
}

bool IFileSystem::GetFileCreationTime(const String16& path, time_t& result) {
    
    return true;
}

MetadataType IFileSystem::GetMetadataType(const String16& path) {
    
    return kMetadataType_Unknown;
}

String16 IFileSystem::GetPathByAppendingComponent(const String16& path, const String16& component) {
    
    return "";
}

bool IFileSystem::CreateDirectory_(const String16& path) {
    
    return true;
}

String16 IFileSystem::GetHomeDirectory() {
    
    return "";
}

String16 IFileSystem::GetFilenameFromPath(const String16& path) {
    
    return "";
}

String16 IFileSystem::GetDirectoryNameFromPath(const String16& path) {
    
    return "";
}

bool IFileSystem::GetVolumeFreeSpace(const String16& path, uint64_t& result) {
    
    return true;
}

int32_t IFileSystem::GetVolumeId(const String16& path) {
    
    return 1;
}

Ref<String16Vector> IFileSystem::ListDirectory(const String16& path, const String16& filter) {
    
    String16 a[3];
    
    Ref<String16Vector> v = String16Vector::Create(a, 3);
    
    return v;
}

String16 IFileSystem::OpenTemporaryFile(const String16& prefix, FileHandle& handle) {
    
    return "";
}

FileHandle IFileSystem::OpenFile(const String16& path, bool open_for_writing) {
    
    return NULL;
}

void IFileSystem::CloseFile(FileHandle& handle) {
    
}

int64_t IFileSystem::SeekFile(FileHandle handle, int64_t offset, FileSeekOrigin origin) {
    
    return 1;
}

bool IFileSystem::TruncateFile(FileHandle handle, int64_t offset) {
    
    return true;
}

int64_t IFileSystem::WriteToFile(FileHandle handle, const char* data, int64_t length) {
    
    return 1;
}

int64_t IFileSystem::ReadFromFile(FileHandle handle, char* data, int64_t length) {
    
    return 1;
}

bool IFileSystem::CopyFile_(const String16& source_path, const String16& destination_path) {
    
    return true;
}

IFileSystem::IFileSystem() {
    
}

IFileSystem::~IFileSystem() {
    
}
