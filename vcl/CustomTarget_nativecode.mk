#
# This file is part of the LibreOffice project.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#

$(eval $(call gb_CustomTarget_CustomTarget,vcl/workben))

wmffuzzer_PYTHONCOMMAND := $(call gb_ExternalExecutable_get_command,python)

wmffuzzer_Native_cxx=$(call gb_CustomTarget_get_workdir,vcl/workben)/native-code.cxx

$(wmffuzzer_Native_cxx): $(SRCDIR)/solenv/bin/native-code.py | $(call gb_CustomTarget_get_workdir,vcl/workben)/.dir
	$(call gb_Helper_abbreviate_dirs, $(wmffuzzer_PYTHONCOMMAND) $(SRCDIR)/solenv/bin/native-code.py -j -g core) > $@

# vim: set noet sw=4 ts=4:
