/*
 *  Copyright (C) 2012-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "SubtitlesSettings.h"

#include "guilib/GUIFontManager.h"
#include "guilib/LocalizeStrings.h"
#include "settings/Settings.h"
#include "settings/lib/Setting.h"
#include "utils/FileUtils.h"
#include "utils/FontUtils.h"
#include "utils/URIUtils.h"

using namespace KODI;
using namespace SUBTITLES;

CSubtitlesSettings::CSubtitlesSettings(const std::shared_ptr<CSettings>& settings)
  : m_settings(settings)
{
  m_settings->RegisterCallback(
      this,
      {CSettings::SETTING_LOCALE_SUBTITLELANGUAGE,  CSettings::SETTING_SUBTITLES_PARSECAPTIONS,
       CSettings::SETTING_SUBTITLES_ALIGN,          CSettings::SETTING_SUBTITLES_STEREOSCOPICDEPTH,
       CSettings::SETTING_SUBTITLES_FONTNAME,       CSettings::SETTING_SUBTITLES_FONTSIZE,
       CSettings::SETTING_SUBTITLES_STYLE,          CSettings::SETTING_SUBTITLES_COLOR,
       CSettings::SETTING_SUBTITLES_BORDERSIZE,     CSettings::SETTING_SUBTITLES_BORDERCOLOR,
       CSettings::SETTING_SUBTITLES_OPACITY,        CSettings::SETTING_SUBTITLES_BGCOLOR,
       CSettings::SETTING_SUBTITLES_BGOPACITY,      CSettings::SETTING_SUBTITLES_BLUR,
       CSettings::SETTING_SUBTITLES_BACKGROUNDTYPE, CSettings::SETTING_SUBTITLES_SHADOWCOLOR,
       CSettings::SETTING_SUBTITLES_SHADOWOPACITY,  CSettings::SETTING_SUBTITLES_SHADOWSIZE,
       CSettings::SETTING_SUBTITLES_MARGINVERTICAL, CSettings::SETTING_SUBTITLES_CHARSET,
       CSettings::SETTING_SUBTITLES_OVERRIDEFONTS,  CSettings::SETTING_SUBTITLES_OVERRIDESTYLES,
       CSettings::SETTING_SUBTITLES_LANGUAGES,      CSettings::SETTING_SUBTITLES_STORAGEMODE,
       CSettings::SETTING_SUBTITLES_CUSTOMPATH,     CSettings::SETTING_SUBTITLES_PAUSEONSEARCH,
       CSettings::SETTING_SUBTITLES_DOWNLOADFIRST,  CSettings::SETTING_SUBTITLES_TV,
       CSettings::SETTING_SUBTITLES_MOVIE,          CSettings::SETTING_SUBTITLES_LINE_SPACING});
}

CSubtitlesSettings::~CSubtitlesSettings()
{
  m_settings->UnregisterCallback(this);
}

void CSubtitlesSettings::OnSettingChanged(const std::shared_ptr<const CSetting>& setting)
{
  if (!setting)
    return;

  SetChanged();
  NotifyObservers(ObservableMessageSettingsChanged);
  if (setting->GetId() == CSettings::SETTING_SUBTITLES_ALIGN)
  {
    SetChanged();
    NotifyObservers(ObservableMessagePositionChanged);
  }
}

Align CSubtitlesSettings::GetAlignment() const
{
  return static_cast<Align>(m_settings->GetInt(CSettings::SETTING_SUBTITLES_ALIGN));
}

void CSubtitlesSettings::SetAlignment(Align align) const
{
  m_settings->SetInt(CSettings::SETTING_SUBTITLES_ALIGN, static_cast<int>(align));
}

HorizontalAlign CSubtitlesSettings::GetHorizontalAlignment() const
{
  return static_cast<HorizontalAlign>(
      m_settings->GetInt(CSettings::SETTING_SUBTITLES_CAPTIONSALIGN));
}

std::string CSubtitlesSettings::GetFontName() const
{
  return m_settings->GetString(CSettings::SETTING_SUBTITLES_FONTNAME);
}

FontStyle CSubtitlesSettings::GetFontStyle() const
{
  return static_cast<FontStyle>(m_settings->GetInt(CSettings::SETTING_SUBTITLES_STYLE));
}

int CSubtitlesSettings::GetFontSize() const
{
  return m_settings->GetInt(CSettings::SETTING_SUBTITLES_FONTSIZE);
}

UTILS::COLOR::Color CSubtitlesSettings::GetFontColor() const
{
  return UTILS::COLOR::ConvertHexToColor(m_settings->GetString(CSettings::SETTING_SUBTITLES_COLOR));
}

int CSubtitlesSettings::GetFontOpacity() const
{
  return m_settings->GetInt(CSettings::SETTING_SUBTITLES_OPACITY);
}

int CSubtitlesSettings::GetBorderSize() const
{
  return m_settings->GetInt(CSettings::SETTING_SUBTITLES_BORDERSIZE);
}

UTILS::COLOR::Color CSubtitlesSettings::GetBorderColor() const
{
  return UTILS::COLOR::ConvertHexToColor(
      m_settings->GetString(CSettings::SETTING_SUBTITLES_BORDERCOLOR));
}

int CSubtitlesSettings::GetShadowSize() const
{
  return m_settings->GetInt(CSettings::SETTING_SUBTITLES_SHADOWSIZE);
}

UTILS::COLOR::Color CSubtitlesSettings::GetShadowColor() const
{
  return UTILS::COLOR::ConvertHexToColor(
      m_settings->GetString(CSettings::SETTING_SUBTITLES_SHADOWCOLOR));
}

int CSubtitlesSettings::GetShadowOpacity() const
{
  return m_settings->GetInt(CSettings::SETTING_SUBTITLES_SHADOWOPACITY);
}

int CSubtitlesSettings::GetBlurSize() const
{
  return m_settings->GetInt(CSettings::SETTING_SUBTITLES_BLUR);
}

int CSubtitlesSettings::GetLineSpacing() const
{
  return m_settings->GetInt(CSettings::SETTING_SUBTITLES_LINE_SPACING);
}

BackgroundType CSubtitlesSettings::GetBackgroundType() const
{
  return static_cast<BackgroundType>(
      m_settings->GetInt(CSettings::SETTING_SUBTITLES_BACKGROUNDTYPE));
}

UTILS::COLOR::Color CSubtitlesSettings::GetBackgroundColor() const
{
  return UTILS::COLOR::ConvertHexToColor(
      m_settings->GetString(CSettings::SETTING_SUBTITLES_BGCOLOR));
}

int CSubtitlesSettings::GetBackgroundOpacity() const
{
  return m_settings->GetInt(CSettings::SETTING_SUBTITLES_BGOPACITY);
}

bool CSubtitlesSettings::IsOverrideFonts() const
{
  return m_settings->GetBool(CSettings::SETTING_SUBTITLES_OVERRIDEFONTS);
}

OverrideStyles CSubtitlesSettings::GetOverrideStyles() const
{
  return static_cast<OverrideStyles>(
      m_settings->GetInt(CSettings::SETTING_SUBTITLES_OVERRIDESTYLES));
}

float CSubtitlesSettings::GetVerticalMarginPerc() const
{
  // We return the vertical margin as percentage
  // to fit the current screen resolution
  return static_cast<float>(m_settings->GetNumber(CSettings::SETTING_SUBTITLES_MARGINVERTICAL));
}

void CSubtitlesSettings::SettingOptionsSubtitleFontsFiller(const SettingConstPtr& setting,
                                                           std::vector<StringSettingOption>& list,
                                                           std::string& current)
{
  // From application system fonts folder we add the default font only
  std::string defaultFontPath =
      URIUtils::AddFileToFolder("special://xbmc/media/Fonts/", UTILS::FONT::FONT_DEFAULT_FILENAME);
  if (CFileUtils::Exists(defaultFontPath))
  {
    std::string familyName = UTILS::FONT::GetFontFamily(defaultFontPath);
    if (!familyName.empty())
    {
      list.emplace_back(g_localizeStrings.Get(571) + " " + familyName, FONT_DEFAULT_FAMILYNAME);
    }
  }
  // Add additional fonts from the user fonts folder
  for (const std::string& familyName : g_fontManager.GetUserFontsFamilyNames())
  {
    list.emplace_back(familyName, familyName);
  }
}
