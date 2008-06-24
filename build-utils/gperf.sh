#! /bin/sh -e
#
# Copyright © 2008 Pierre Habouzit <madcoder@debian.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#


die() {
    echo "$@" 1>&2
    exit 2
}

do_hdr() {
    cat <<EOF
/* This file is autogenerated by $(basename $0) */

EOF
}

out=
type_t=

while true; do
    test $# -gt 2 || break
    case "$1" in
        -o) shift; out="$1"; shift;;
        -t) shift; type_t="$1"; shift;;
        *)  break;;
    esac
done

do_h() {
    cat <<EOF
`do_hdr`
#ifndef AWESOME_TOKENIZE_H
#define AWESOME_TOKENIZE_H

typedef enum awesome_token_t {
    A_TK_UNKNOWN,
`tr 'a-z-./ ' 'A-Z____' | sed -e "s/^[^/].*/    A_TK_&,/"`
} awesome_token_t;

__attribute__((pure)) enum awesome_token_t a_tokenize(const char *s, int len);
#endif
EOF
}

do_tokens() {
    while read tok; do
        case "$tok" in
            "") continue;;
            *)
                echo "$tok, A_TK_`echo $tok | tr 'a-z-./ ' 'A-Z____'`"
                ;;
        esac
    done
}

do_c() {
    if ! which gperf > /dev/null; then
        echo "gperf not found. You need to install gperf." > /dev/stderr;
        exit 1;
    fi;

    cat <<EOF | gperf -m16 -l -t -C -F",0" \
        --language=ANSI-C -Na_tokenize_aux \
        | sed -e '/__gnu_inline__/d;s/\<\(__\|\)inline\>//g'
%{
`do_hdr`

#include <string.h>
#include "common/tokenize.h"

static const struct tok *a_tokenize_aux(const char *str, unsigned int len);

%}
struct tok { const char *name; int val; };
%%
`do_tokens`
%%

awesome_token_t a_tokenize(const char *s, int len)
{
    if (len < 0)
        len = (int)strlen(s);

    if (len) {
        const struct tok *res = a_tokenize_aux(s, len);
        return res ? res->val : A_TK_UNKNOWN;
    } else {
        return A_TK_UNKNOWN;
    }
}
EOF
}

extract_tokens() {
    grep '^### ' "$1" | cut -d ' ' -f 2
}


TOKENS_FILE="$1"
TARGET="$2"

trap "rm -f ${TARGET}" 0

rm -f "${TARGET}"
case "${TARGET}" in
    *.h) cat "${TOKENS_FILE}" | do_h > "${TARGET}";;
    *.c) cat "${TOKENS_FILE}" | do_c > "${TARGET}";;
    *)  die "you must ask for the 'h' or 'c' generation";;
esac
chmod -w "${TARGET}"

trap - 0
exit 0