#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

: ${OGIT:=$(realpath $(atf_get_srcdir)/../ogit)}

atf_test_case log
log_head()
{

}

log_body()
{

	wrkdir=$(realpath .)
	mkdir foo
	cd foo
	git init
	touch bar
	git add bar
	git commit -m "Initial Commit.$"
	${OGIT} log --color=never > ${wrkdir}/.log

	lines=$(cat ${wrkdir}/.log | wc -l | tr -d '[:space:]')
	commithash=$(head -1 ${wrkdir}/.log | sed -e 's/commit //')
	expectedhash=$(cat .git/refs/heads/master)

	echo ${commithash} | head -1
	atf_check_equal "${commithash}" "${expectedhash}"

	atf_check -x "head -2 ${wrkdir}/.log | tail -1 | grep -qe '^Author:'"
	atf_check -x "head -3 ${wrkdir}/.log | tail -1 | grep -qEe '^Date:.+[+-][0-9]{4}$'"
	atf_check -x "head -4 ${wrkdir}/.log | tail -1 | grep -qe '^$'"
	atf_check -x "grep -q '^Initial Commit.$\$' ${wrkdir}/.log"
	atf_check_equal "${lines}" "5"

	touch foo
	touch baz
	git add foo baz
	git commit -m "Second commit."
	${OGIT} log --color=never > ${wrkdir}/.log

	atf_check -x "head -2 ${wrkdir}/.log | tail -1 | grep -qe '^Author:'"
	atf_check -x "head -3 ${wrkdir}/.log | tail -1 | grep -qEe '^Date:.+[+-][0-9]{4}$'"
	atf_check -x "head -4 ${wrkdir}/.log | tail -1 | grep -qe '^$'"
}

atf_init_test_cases()
{
	# We'll use GPL-licensed git to create our repos for sanity checking
	atf_require_prog git

	atf_add_test_case log
}
