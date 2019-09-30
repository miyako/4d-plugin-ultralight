//
//  IFileSystem.cpp
//  Ultralight
//
//  Created by miyako on 2019/09/27.
//  Copyright © 2019 miyako. All rights reserved.
//

#include "IFileSystem.hpp"

/// @note  To support File URL loading, you ONLY need to implement the
///        following functions:
///          - FileSystem::FileExists
///          - FileSystem::GetFileSize
///          - FileSystem::GetFileMimeType
///          - FileSystem::OpenFile
///          - FileSystem::ReadFromFile
///          - FileSystem::CloseFile

bool IFileSystem::FileExists(const String16& p) {
    
    bool fileExists = false;
    
#if VERSIONMAC
    if(p.length()) {
        NSString *path = [[NSString alloc]initWithCharacters:(const unichar *)p.data() length:p.length()];
        NSFileManager *fileManager = [[NSFileManager alloc]init];
        /* return true if exists */
        fileExists = [fileManager fileExistsAtPath:path isDirectory:NULL];
        [fileManager release];
        [path release];
    }
#endif
    
    return fileExists;
}

bool IFileSystem::DeleteFile_(const String16& path) {
    
    /* return true on success */
    return false;
}

bool IFileSystem::DeleteEmptyDirectory(const String16& path) {
    
    /* return true on success */
    return false;
}

bool IFileSystem::MoveFile_(const String16& old_path, const String16& new_path) {
    
    /* return true on success */
    return false;
}

bool IFileSystem::GetFileSize(const String16& p, int64_t& result) {
    
#if VERSIONMAC
    if(p.length()) {
        
        NSString *path = [[NSString alloc]initWithCharacters:(const unichar *)p.data() length:p.length()];
        NSFileManager *fileManager = [[NSFileManager alloc]init];
        NSDictionary *attributes = [fileManager attributesOfItemAtPath:path error:NULL];
        result = [attributes fileSize];
        [fileManager release];
        [path release];
        /* Return true on success */
        return true;
    }
    
#endif
    
    return false;
}

bool IFileSystem::GetFileSize(FileHandle handle, int64_t& result) {
    
#if VERSIONMAC
    if(handle) {
        NSFileHandle *h = [[NSFileHandle alloc]initWithFileDescriptor:handle];
        int64_t pos = [h offsetInFile];
        [h seekToEndOfFile];
        result = [h offsetInFile];
        [h seekToFileOffset:pos];
        [h release];
        
        /* Return true on success */
        return true;
    }
    
#endif
    
    return false;
}

bool IFileSystem::GetFileMimeType(const String16& p, String16& result) {

    bool success = false;
    
#if VERSIONMAC
    if(p.length()) {
        NSString *path = [[NSString alloc]initWithCharacters:(const unichar *)p.data() length:p.length()];
        NSString *pathExtension = [path pathExtension];
        NSString *uti = (NSString *)UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
                                                                          (CFStringRef)pathExtension, NULL);
        if(uti) {
            NSString *mime = (NSString *)UTTypeCopyPreferredTagWithClass((CFStringRef)uti, kUTTagClassMIMEType);
            if(mime) {
                std::vector<unichar> buf([mime length]+1);
                [mime getCharacters:&buf[0] range:NSMakeRange(0, [mime length])];
                result = String16((unsigned short *)&buf[0], [mime length]);
                /* Return true on success */
                success = true;
                
            }
            [uti release];
        }

        [path release];
    }
#endif
    
    return success;
}

bool IFileSystem::GetFileModificationTime(const String16& path, time_t& result) {
    
    /* Return true on success */
    return false;
}

bool IFileSystem::GetFileCreationTime(const String16& path, time_t& result) {
    
    /* Return true on success */
    return false;
}

MetadataType IFileSystem::GetMetadataType(const String16& path) {
    
    /* Get path type (file or directory) */
    return kMetadataType_Unknown;
}

String16 IFileSystem::GetPathByAppendingComponent(const String16& path, const String16& component) {
    
    return "";
}

bool IFileSystem::CreateDirectory_(const String16& path) {
    
    /* return true on success */
    return false;
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
    
    /* Return true on success */
    return false;
}

int32_t IFileSystem::GetVolumeId(const String16& path) {
    
    /* return its unique volume id */
    return 0;
}

Ref<String16Vector> IFileSystem::ListDirectory(const String16& path, const String16& filter) {
    
    String16 a[3];
    
    Ref<String16Vector> v = String16Vector::Create(a, 3);
    
    return v;
}

String16 IFileSystem::OpenTemporaryFile(const String16& prefix, FileHandle& handle) {
    
    /* Return path of temporary file */
    return "";
}

FileHandle IFileSystem::OpenFile(const String16& p, bool open_for_writing) {

    FileHandle h = NULL;
    
#if VERSIONMAC
    if(p.length()) {
        NSString *path = [[NSString alloc]initWithCharacters:(const unichar *)p.data() length:p.length()];
        
        if(![path hasPrefix:@"/"]) {
            NSString *u = path;
            path = [[NSString alloc]initWithFormat:@"/%@", u];
            [u release];
        }
        
        h = open_for_writing
        ? [[NSFileHandle fileHandleForWritingAtPath:path]fileDescriptor]
        : [[NSFileHandle fileHandleForReadingAtPath:path]fileDescriptor];
        [path release];
    }

#endif
    
    return h;
}

void IFileSystem::CloseFile(FileHandle& handle) {
    
#if VERSIONMAC
    if(handle) {
        NSFileHandle *h = [[NSFileHandle alloc]initWithFileDescriptor:handle];
        [h closeFile];
        [h release];
    }
#endif

}

int64_t IFileSystem::SeekFile(FileHandle handle, int64_t offset, FileSeekOrigin origin) {
 
    int64_t pos = 0;
    
#if VERSIONMAC
    if(handle) {
        NSFileHandle *h = [[NSFileHandle alloc]initWithFileDescriptor:handle];
        switch (origin) {
            case kFileSeekOrigin_Beginning:
                 [h seekToFileOffset:offset];
                break;
            case kFileSeekOrigin_Current:
                [h seekToFileOffset:[h offsetInFile] + offset];
                break;
            case kFileSeekOrigin_End:
                [h seekToEndOfFile];
                [h seekToFileOffset:[h offsetInFile] + offset];
                break;
        }
        /* Return new file offset */
        pos = [h offsetInFile];

        [h release];
    }
#endif
    
    return pos;
}

bool IFileSystem::TruncateFile(FileHandle handle, int64_t offset) {
    
    /* return true on success */
    return false;
}

int64_t IFileSystem::WriteToFile(FileHandle handle, const char* data, int64_t length) {
    
    /* return number of bytes written or -1 on failure */
    return 0;
}

int64_t IFileSystem::ReadFromFile(FileHandle handle, char* p, int64_t length) {
    
    int64_t len = 0;
    
#if VERSIONMAC
    if(handle) {
        NSFileHandle *h = [[NSFileHandle alloc]initWithFileDescriptor:handle];
        NSData *data = [h readDataOfLength:length];
        /* return number of bytes read or -1 on failure */
        if(data){
            len = [data length];
            [data getBytes:p length:len];
        }else{
            len = -1;
        }

        [h release];
    }
#endif
    
    return len;
}

bool IFileSystem::CopyFile_(const String16& source_path, const String16& destination_path) {
    
    /* return true on success */
    return false;
}

IFileSystem::IFileSystem() {
    
}

IFileSystem::~IFileSystem() {
    
}
