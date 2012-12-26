dnl Check for YAJL.
dnl CHECK_YAJL([CALL-ON-SUCCESS [, CALL-ON-FAILURE]])
dnl Sets:
dnl YAJL_CFLAGS
dnl YAJL_LDFLAGS
dnl HAVE_YAJL

HAVE_YAJL="no"
YAJL_CFlAGS=
YAJL_LDFLAGS=

AC_DEFUN([CHECK_YAJL],
[dnl

AC_ARG_WITH(
    yajl,
    [AC_HELP_STRING([--with-yajl=PATH], [Path to yajl])],
    [test_paths="${with_yajl}"
     require_yajl="yes"],
    [test_paths="/usr/local /opt/local /opt /usr"
     require_yajl="no"])

save_LDFLAGS="$LDFLAGS"
save_CFLAGS="$CFLAGS"

if test "${test_paths}" != "no"; then
    yajl_path=""
    for x in ${test_paths}; do

        AC_MSG_CHECKING([yajl in ${x}])
        CFLAGS="-I${x}/include"
        LDFLAGS="-L${x}/$libsubdir -lyajl"

        AC_LANG([C])
        AC_COMPILE_IFELSE(
            [AC_LANG_PROGRAM(
                [[
                    #include <yajl/yajl_parse.h>
                    #include <yajl/yajl_gen.h>
                    #include <yajl/yajl_tree.h>
                ]],
                [[
                    yajl_gen g = yajl_gen_alloc(NULL);
                    yajl_handle h = yajl_alloc(NULL, NULL, NULL);
                    yajl_parse(h, "{\"k\":\"v\"}", 9);
                    yajl_free(h);
                    yajl_gen_free(g);
                ]]
            )],
            [dnl
                AC_MSG_RESULT([yes])
                HAVE_YAJL=yes
                LDFLAGS="$save_LDFLAGS $YAJL_LDFLAGS"
                CFLAGS="$save_CFLAGS $YAJL_CFLAGS"
                $1
                break
            ],
            [dnl
                AC_MSG_RESULT([no])
                HAVE_YAJL=no
                LDFLAGS="$save_LDFLAGS"
                CFLAGS="$save_CFLAGS"
                $2
            ])
    done

    dnl # Fail if the user asked for YAJL explicitly.
    if test "${require_yajl}" == yes && test "${HAVE_YAJL}" != "yes"; then
        AC_MSG_ERROR([not found])
    fi
fi

AC_SUBST(HAVE_YAJL)
AC_SUBST(YAJL_CFLAGS)
AC_SUBST(YAJL_LDFLAGS)
])