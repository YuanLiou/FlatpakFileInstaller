// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "packageinfo.h"
#include "permissions.h"

#include <QString>

// Plain-language, translated text about where an app comes from and what
// it may access. Written for people who know nothing about Linux: no
// jargon, and nothing that sounds alarming.
namespace PermissionText
{

QString describe(const Permissions::Permission &permission);

QString sourceTitle(const PackageInfo &package);
QString sourceExplanation(const PackageInfo &package);
bool sourceNeedsTrust(const PackageInfo &package);

} // namespace PermissionText
