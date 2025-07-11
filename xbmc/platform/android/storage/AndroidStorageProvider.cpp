/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "AndroidStorageProvider.h"

#include "Util.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "guilib/LocalizeStrings.h"
#include "utils/RegExp.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include "platform/android/activity/XBMCApp.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include <androidjni/Context.h>
#include <androidjni/Environment.h>
#include <androidjni/File.h>
#include <androidjni/StatFs.h>
#include <androidjni/StorageManager.h>
#include <androidjni/StorageVolume.h>

namespace
{

// clang-format off
constexpr std::array<const char*, 10> typeWL = {
  "vfat",
  "exfat",
  "sdcardfs",
  "fuse",
  "ntfs",
  "fat32",
  "ext3",
  "ext4",
  "esdfs",
  "cifs"
};

constexpr std::array<const char*, 3> mountWL = {
  "/mnt",
  "/Removable",
  "/storage"
};

constexpr std::array<const char*, 9> mountBL = {
  "/mnt/secure",
  "/mnt/shell",
  "/mnt/asec",
  "/mnt/obb",
  "/mnt/media_rw/extSdCard",
  "/mnt/media_rw/sdcard",
  "/mnt/media_rw/usbdisk",
  "/storage/emulated",
  "/mnt/runtime"
};

constexpr std::array<const char*, 4> deviceWL = {
  "/dev/block/vold",
  "/dev/fuse",
  "/mnt/media_rw",
  "//" // SMB
};
// clang-format on

} // namespace

std::unique_ptr<IStorageProvider> IStorageProvider::CreateInstance()
{
  return std::make_unique<CAndroidStorageProvider>();
}

CAndroidStorageProvider::CAndroidStorageProvider()
{
  PumpDriveChangeEvents(NULL);
}

std::string CAndroidStorageProvider::unescape(const std::string& str)
{
  std::string retString;
  for (uint32_t i=0; i < str.length(); ++i)
  {
    if (str[i] != '\\')
      retString += str[i];
    else
    {
      i += 1;
      if (str[i] == 'u') // unicode
      {
        //! @todo implement
      }
      else if (str[i] >= '0' && str[i] <= '7') // octal
      {
        std::string octString;
        while (str[i] >= '0' && str[i] <= '7')
        {
          octString += str[i];
          i += 1;
        }
        if (!octString.empty())
        {
          uint8_t val = 0;
          for (int j=octString.length()-1; j>=0; --j)
          {
            val += ((uint8_t)(octString[j] - '0')) * (1 << ((octString.length() - (j+1)) * 3));
          }
          retString += (char)val;
          i -= 1;
        }
      }
    }
  }
  return retString;
}

void CAndroidStorageProvider::GetLocalDrives(std::vector<CMediaSource>& localDrives)
{
  CMediaSource share;

  // external directory
  std::string path;
  if (GetExternalStorage(path) && !path.empty() && XFILE::CDirectory::Exists(path))
  {
    share.strPath = path;
    share.strName = g_localizeStrings.Get(21456);
    share.m_ignore = true;
    localDrives.push_back(share);
  }

  // root directory
  share.strPath = "/";
  share.strName = g_localizeStrings.Get(21453);
  localDrives.push_back(share);
}

void CAndroidStorageProvider::GetRemovableDrives(std::vector<CMediaSource>& removableDrives)
{
  bool inError = false;

  CJNIStorageManager manager(CJNIContext::getSystemService(CJNIContext::STORAGE_SERVICE));
  if (xbmc_jnienv()->ExceptionCheck())
  {
    xbmc_jnienv()->ExceptionDescribe();
    xbmc_jnienv()->ExceptionClear();
    inError = true;
  }

  if (!inError)
  {
    CJNIStorageVolumes vols = manager.getStorageVolumes();
    if (xbmc_jnienv()->ExceptionCheck())
    {
      xbmc_jnienv()->ExceptionDescribe();
      xbmc_jnienv()->ExceptionClear();
      inError = true;
    }

    if (!inError)
    {
      std::vector<CMediaSource> droidDrives;

      for (int i = 0; i < vols.size(); ++i)
      {
        CJNIStorageVolume vol = vols.get(i);
        // CLog::Log(LOGDEBUG, "-- Volume: {}({}) -- {}", vol.getDirectory().getAbsolutePath(), vol.getUserLabel(), vol.getState());

        bool removable = vol.isRemovable();
        if (xbmc_jnienv()->ExceptionCheck())
        {
          xbmc_jnienv()->ExceptionDescribe();
          xbmc_jnienv()->ExceptionClear();
          inError = true;
          break;
        }

        std::string state = vol.getState();
        if (xbmc_jnienv()->ExceptionCheck())
        {
          xbmc_jnienv()->ExceptionDescribe();
          xbmc_jnienv()->ExceptionClear();
          inError = true;
          break;
        }

        if (removable && state == CJNIEnvironment::MEDIA_MOUNTED)
        {
          CMediaSource share;

          if (CJNIBase::GetSDKVersion() >= 30)
            share.strPath = vol.getDirectory().getAbsolutePath();
          else
            share.strPath = vol.getPath();
          if (xbmc_jnienv()->ExceptionCheck())
          {
            xbmc_jnienv()->ExceptionDescribe();
            xbmc_jnienv()->ExceptionClear();
            inError = true;
            break;
          }

          share.strName = vol.getUserLabel();
          if (xbmc_jnienv()->ExceptionCheck())
          {
            xbmc_jnienv()->ExceptionDescribe();
            xbmc_jnienv()->ExceptionClear();
            inError = true;
            break;
          }

          StringUtils::Trim(share.strName);
          if (share.strName.empty() || share.strName == "?" ||
              StringUtils::EqualsNoCase(share.strName, "null"))
            share.strName = URIUtils::GetFileName(share.strPath);

          share.m_ignore = true;
          droidDrives.emplace_back(share);
        }
      }

      if (!inError)
      {
        removableDrives.insert(removableDrives.end(), droidDrives.begin(), droidDrives.end());
        return;
      }
    }
  }

  // Try fallback for SDK < 24 or in case of error
  for (const auto& mountStr : GetRemovableDrivesLinux())
  {
    // Reject unreadable
    if (XFILE::CDirectory::Exists(mountStr))
    {
      CMediaSource share;
      share.strPath = unescape(mountStr);
      share.strName = URIUtils::GetFileName(mountStr);
      share.m_ignore = true;
      removableDrives.emplace_back(share);
    }
  }
}

std::set<std::string> CAndroidStorageProvider::GetRemovableDrivesLinux()
{
  std::set<std::string> result;

  // mounted usb disks
  char*                               buf     = NULL;
  FILE*                               pipe;
  CRegExp                             reMount;
  reMount.RegComp("^(.+?)\\s+(.+?)\\s+(.+?)\\s+(.+?)\\s");

  /* /proc/mounts is only guaranteed atomic for the current read
   * operation, so we need to read it all at once.
   */
  if ((pipe = fopen("/proc/mounts", "r")))
  {
    char*   new_buf;
    size_t  buf_len = 4096;

    while ((new_buf = (char*)realloc(buf, buf_len * sizeof(char))))
    {
      size_t nread;

      buf   = new_buf;
      nread = fread(buf, sizeof(char), buf_len, pipe);

      if (nread == buf_len)
      {
        rewind(pipe);
        buf_len *= 2;
      }
      else
      {
        buf[nread] = '\0';
        if (!feof(pipe))
          new_buf = NULL;
        break;
      }
    }

    if (!new_buf)
    {
      free(buf);
      buf = NULL;
    }
    fclose(pipe);
  }
  else
    CLog::Log(LOGERROR, "Cannot read mount points");

  if (buf)
  {
    char* line;
    char* saveptr = NULL;

    line = strtok_r(buf, "\n", &saveptr);

    while (line)
    {
      if (reMount.RegFind(line) != -1)
      {
        std::string deviceStr   = reMount.GetReplaceString("\\1");
        std::string mountStr = reMount.GetReplaceString("\\2");
        std::string fsStr    = reMount.GetReplaceString("\\3");
        std::string optStr    = reMount.GetReplaceString("\\4");

        // Blacklist
        bool bl_ok = true;

        // What mount points are rejected
        for (const auto& mount : mountBL)
        {
          if (StringUtils::StartsWithNoCase(mountStr, mount))
          {
            bl_ok = false;
            break;
          }
        }

        if (bl_ok)
        {
          // What filesystems are accepted
          bool fsok = false;
          for (const auto& type : typeWL)
          {
            if (StringUtils::StartsWithNoCase(fsStr, type))
            {
              fsok = true;
              break;
            }
          }
          // What devices are accepted
          bool devok = false;
          for (const auto& device : deviceWL)
          {
            if (StringUtils::StartsWithNoCase(deviceStr, device))
            {
              devok = true;
              break;
            }
          }

          // What mount points are accepted
          bool mountok = false;
          for (const auto& mount : mountWL)
          {
            if (StringUtils::StartsWithNoCase(mountStr, mount))
            {
              mountok = true;
              break;
            }
          }

          if(devok && (fsok || mountok))
          {
            result.insert(mountStr);
          }
        }
      }
      line = strtok_r(NULL, "\n", &saveptr);
    }
    free(buf);
  }
  return result;
}

std::vector<std::string> CAndroidStorageProvider::GetDiskUsage()
{
  std::vector<std::string> result;

  std::string usage;
  // add header
  GetStorageUsage("", usage);
  result.push_back(usage);

  usage.clear();
  // add rootfs
  if (GetStorageUsage("/", usage) && !usage.empty())
    result.push_back(usage);

  usage.clear();
  // add external storage if available
  std::string path;
  if (GetExternalStorage(path) && !path.empty() && GetStorageUsage(path, usage) && !usage.empty())
    result.push_back(usage);

  // add removable storage
  std::vector<CMediaSource> drives;
  GetRemovableDrives(drives);
  for (unsigned int i = 0; i < drives.size(); i++)
  {
    usage.clear();
    if (GetStorageUsage(drives[i].strPath, usage) && !usage.empty())
      result.push_back(usage);
  }

  return result;
}

bool CAndroidStorageProvider::PumpDriveChangeEvents(IStorageEventsCallback *callback)
{
  std::vector<CMediaSource> drives;
  GetRemovableDrives(drives);
  bool changed = m_removableDrives != drives;
  m_removableDrives = std::move(drives);
  return changed;
}

namespace
{
constexpr float GIGABYTES = 1073741824;
constexpr int PATH_MAXLEN = 38;
} // namespace

bool CAndroidStorageProvider::GetStorageUsage(const std::string& path, std::string& usage)
{
  if (path.empty())
  {
    usage = StringUtils::Format("{:<{}}{:>12}{:>12}{:>12}{:>12}", "Filesystem", PATH_MAXLEN, "Size",
                                "Used", "Avail", "Use %");
    return false;
  }

  CJNIStatFs fileStat(path);
  const int blockSize = fileStat.getBlockSize();
  const int blockCount = fileStat.getBlockCount();
  const int freeBlocks = fileStat.getFreeBlocks();

  if (blockSize <= 0 || blockCount <= 0 || freeBlocks < 0)
    return false;

  const float totalSize = static_cast<float>(blockSize) * blockCount / GIGABYTES;
  const float freeSize = static_cast<float>(blockSize) * freeBlocks / GIGABYTES;
  const float usedSize = totalSize - freeSize;
  const float usedPercentage = usedSize / totalSize * 100;

  usage = StringUtils::Format(
      "{:<{}}{:>11.1f}{}{:>11.1f}{}{:>11.1f}{}{:>11.0f}{}",
      path.size() < PATH_MAXLEN - 1 ? path : StringUtils::Left(path, PATH_MAXLEN - 4) + "...",
      PATH_MAXLEN, totalSize, "G", usedSize, "G", freeSize, "G", usedPercentage, "%");
  return true;
}

bool CAndroidStorageProvider::GetExternalStorage(std::string& path,
                                                 const std::string& type /* = "" */)
{
  std::string sType;
  std::string mountedState;
  bool mounted = false;

  if (CJNIBase::GetSDKVersion() <= 29)
  {
    CJNIFile external = CJNIEnvironment::getExternalStorageDirectory();
    if (external)
      path = external.getAbsolutePath();
  }
  else
  {
    CJNIStorageManager manager =
        (CJNIStorageManager)CXBMCApp::Get().getSystemService(CJNIContext::STORAGE_SERVICE);
    CJNIStorageVolume volume = manager.getPrimaryStorageVolume();
    path = volume.getDirectory().getAbsolutePath();
  }

  if (type == "music")
    sType = CJNIEnvironment::DIRECTORY_MUSIC;
  else if (type == "videos")
    sType = CJNIEnvironment::DIRECTORY_MOVIES;
  else if (type == "pictures")
    sType = CJNIEnvironment::DIRECTORY_PICTURES;
  else if (type == "photos")
    sType = CJNIEnvironment::DIRECTORY_DCIM;
  else if (type == "downloads")
    sType = CJNIEnvironment::DIRECTORY_DOWNLOADS;

  if (!sType.empty())
  {
    path.append("/");
    path.append(sType);
  }

  mountedState = CJNIEnvironment::getExternalStorageState();
  mounted = (mountedState == CJNIEnvironment::MEDIA_MOUNTED ||
             mountedState == CJNIEnvironment::MEDIA_MOUNTED_READ_ONLY);
  return mounted && !path.empty();
}
