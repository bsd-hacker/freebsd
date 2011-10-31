# $FreeBSD$

setup()
{
	enable_val=$(sysctl -n vfs.varsym.enable)
	allow_default_val=$(sysctl -n vfs.varsym.allow_default)
	max_proc_setsize_val=$(sysctl -n vfs.varsym.max_proc_setsize)
	set_proc_val=$(sysctl -n security.bsd.unprivileged_varsym_set_proc)

	echo aaa > aaa
	echo bbb > bbb
	echo ccc > ccc
	rm -f xxx
	ln -s '${XXX:ccc}' xxx

	varsym -is
}

teardown()
{
	sysctl vfs.varsym.enable=${enable_val} > /dev/null
	sysctl vfs.varsym.allow_default=${allow_default_val} > /dev/null
	sysctl vfs.varsym.max_proc_setsize=${max_proc_setsize_val} > /dev/null
	sysctl security.bsd.unprivileged_varsym_set_proc=${set_proc_val} > \
	    /dev/null

	rm -f aaa bbb ccc xxx
}

echo 1..15

REGRESSION_START($1)

if [ $(id -u) -ne 0 ]; then
	REGRESSION_FATAL(1, `must be run as root')
fi

if ! sysctl -Nq vfs.varsym.enable > /dev/null; then
	REGRESSION_FATAL(1, `varsym not in kernel')
fi

setup
PREEXITCMD=teardown 

sysctl vfs.varsym.enable=1 > /dev/null
sysctl vfs.varsym.allow_default=0 > /dev/null

REGRESSION_TEST(`all_unset', `(varsym -i cat xxx 2>&1 || true)')

sysctl vfs.varsym.allow_default=1 > /dev/null
REGRESSION_TEST(`all_unset_default', `(cat xxx 2>&1)')
sysctl vfs.varsym.allow_default=0 > /dev/null

REGRESSION_TEST(`proc_basic', `(varsym -i XXX=aaa cat xxx)')

varsym -s XXX=aaa

REGRESSION_TEST(`sys_basic', `(varsym -i cat xxx 2>&1)')
REGRESSION_TEST(`sys_override', `(varsym -i XXX=bbb cat xxx 2>&1)')

REGRESSION_TEST(`list_basic', `(varsym -i XXX=bbb YYY=ccc ZZZ=ddd varsym)')
REGRESSION_TEST(`list_clear_proc',
    `(varsym -i XXX=bbb YYY=ccc ZZZ=ddd varsym -i varsym)')

sysctl vfs.varsym.enable=0 > /dev/null
REGRESSION_TEST(`disabled_list', `varsym 2>&1')
REGRESSION_TEST(`disabled_get', `varsym -P XXX 2>&1')
REGRESSION_TEST(`disabled_set', `varsym XXX=yyy 2>&1')
sysctl vfs.varsym.enable=1 > /dev/null

varsym -si

REGRESSION_TEST(`sys_cleared', `(varsym -i cat xxx 2>&1 || true)')

varsym -s XXX=aaa

REGRESSION_TEST(`printenv_override', `(varsym -i XXX=bbb varsym -P XXX)')
REGRESSION_TEST(`printenv_proc', `(varsym -i XXX=bbb varsym -Pp XXX)')
REGRESSION_TEST(`printenv_sys', `(varsym -i XXX=bbb varsym -Ps XXX)')

varsym -sd XXX

REGRESSION_TEST(`sys_deleted', `(varsym -i cat xxx 2>&1 || true)')

sysctl security.bsd.unprivileged_varsym_set_proc=1 > /dev/null
REGRESSION_TEST(`nobody_set', `(su -m nobody -c "varsym -i XXX=aaa cat xxx")')

sysctl vfs.varsym.max_proc_setsize=1 > /dev/null
REGRESSION_TEST(`nobody_set_toomany', \
    `(su -m nobody -c "varsym -i XXX=aaa YYY=bbb cat xxx" 2>&1)')
sysctl vfs.varsym.max_proc_setsize=${max_proc_setsize_val} > /dev/null

sysctl security.bsd.unprivileged_varsym_set_proc=0 > /dev/null
REGRESSION_TEST(`nobody_set_denied', \
    `(su -m nobody -c "varsym -i XXX=aaa cat xxx" 2>&1)')
sysctl security.bsd.unprivileged_varsym_set_proc=1 > /dev/null

REGRESSION_END()
