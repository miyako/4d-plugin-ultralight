#!/bin/sh
#DEVELOPER_ID_APPLICATION_CODE_SIGN_IDENTITY_NAME=`security find-identity -p codesigning -v | egrep 'Developer ID Application[^"]+' -o`
codesign -f -s "$EXPANDED_CODE_SIGN_IDENTITY_NAME" "$CODESIGNING_FOLDER_PATH/Contents/manifest.json"
codesign -f -s "$EXPANDED_CODE_SIGN_IDENTITY_NAME" "$CODESIGNING_FOLDER_PATH/Contents/MacOS/libUltralightCore.dylib"
codesign -f -s "$EXPANDED_CODE_SIGN_IDENTITY_NAME" "$CODESIGNING_FOLDER_PATH/Contents/MacOS/libUltralight.dylib"
codesign -f -s "$EXPANDED_CODE_SIGN_IDENTITY_NAME" "$CODESIGNING_FOLDER_PATH/Contents/MacOS/libAppCore.dylib"
codesign -f -s "$EXPANDED_CODE_SIGN_IDENTITY_NAME" "$CODESIGNING_FOLDER_PATH/Contents/MacOS/libWebCore.dylib"

