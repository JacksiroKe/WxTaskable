//  TimesheetsTracker is a desktop that aids you in tracking your timesheets
//  and seeing what work you have done.
//
//  Copyright(C) <2018> <Szymon Welgus>
//
//  This program is free software : you can redistribute it and /
//  or modify it under the terms of the GNU General Public License as published
//  by the Free Software Foundation
//  , either version 3 of the License
//  , or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful
//  , but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <wx/string.h>
#include <wx/stdpaths.h>
#include <wx/fileconf.h>

namespace app::cfg
{
class Configuration
{
public:
    Configuration();
    ~Configuration();

    void Save();

    wxString GetConnectionString() const;
    void SetConnectionString(const wxString& value);

    bool IsConfirmOnExit() const;
    void SetConfirmOnExit(bool value);

    bool IsStartOnBoot() const;
    void SetStartOnBoot(bool value);

    bool IsMinimizeToTray() const;
    void SetMinimizeToTray(bool value);

    bool IsCloseToTray() const;
    void SetCloseToTray(bool value);

    bool IsShowBalloonNotifications() const;
    void SetShowBalloonNotifications(bool value);

    bool IsBackupEnabled() const;
    void SetBackupEnabled(bool value);

    wxString GetBackupPath() const;
    void SetBackupPath(const wxString& value);

private:
    static wxString CFG_FILE;

    template<class T>
    T Get(const wxString& group, const wxString& key, T defaultValue) const;

    template<class T>
    void Set(const wxString& group, const wxString& key, T value);

    wxFileConfig* mConfig;
};
}
