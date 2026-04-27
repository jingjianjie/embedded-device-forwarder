#!/usr/bin/env python3
"""scripts/deploy_target.py — dev → target (Orange Pi 5 Plus) 部署 helper。

对应 RPD §6.5 计划的 deploy_target 工具(用 Python 替代 bash,因为 paramiko
比 sshpass 在 Windows 上更可靠 + 自带 SFTP 流式写绕开 paramiko 4.0
sftp.put() 在 Windows + Armbian sshd 组合下偶发 ENOENT 的 bug)。

用法:
  python3 scripts/deploy_target.py exec "uname -a"
  python3 scripts/deploy_target.py put <local-file> <remote-path>
  python3 scripts/deploy_target.py put_dir <local-dir> <remote-dir>

target 凭据见 PROJECT_CONTEXT.MD / doc/RPD.md §6.1(测试机随用,无安全要求)。
"""
import sys, os
import paramiko
from pathlib import Path

HOST = "192.168.31.6"
USER = "root"
PWD  = "orangepi"

def get_ssh():
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(HOST, username=USER, password=PWD, timeout=10)
    return c

def cmd_exec(args):
    c = get_ssh()
    stdin, stdout, stderr = c.exec_command(args[0], timeout=120)
    rc = stdout.channel.recv_exit_status()
    out = stdout.read().decode("utf-8", errors="replace")
    err = stderr.read().decode("utf-8", errors="replace")
    print(out, end="")
    if err:
        print("---STDERR---", file=sys.stderr)
        print(err, file=sys.stderr, end="")
    sys.exit(rc)

def cmd_put(args):
    local, remote = args
    c = get_ssh()
    sftp = c.open_sftp()
    # paramiko 4.0 sftp.put() 在某些环境失败,流式写绕过
    with open(local, "rb") as src, sftp.open(remote, "wb") as dst:
        dst.write(src.read())
    print(f"PUT {local} -> {remote}")
    sftp.close()
    c.close()

def cmd_put_dir(args):
    local_dir, remote_dir = args
    local = Path(local_dir).resolve()
    c = get_ssh()
    sftp = c.open_sftp()
    # mkdir -p remote_dir
    parts = remote_dir.strip("/").split("/")
    p = ""
    for part in parts:
        p += "/" + part
        try: sftp.mkdir(p)
        except: pass
    count = 0
    for f in local.rglob("*"):
        if f.is_dir(): continue
        rel = f.relative_to(local).as_posix()
        rpath = remote_dir.rstrip("/") + "/" + rel
        # ensure parent
        rdir = "/".join(rpath.split("/")[:-1])
        sub = ""
        for part in rdir.strip("/").split("/"):
            sub += "/" + part
            try: sftp.mkdir(sub)
            except: pass
        with open(str(f), "rb") as src, sftp.open(rpath, "wb") as dst:
            dst.write(src.read())
        count += 1
    print(f"PUT_DIR done, {count} files -> {remote_dir}")
    sftp.close(); c.close()

if __name__ == "__main__":
    op = sys.argv[1]
    rest = sys.argv[2:]
    {"exec": cmd_exec, "put": cmd_put, "put_dir": cmd_put_dir}[op](rest)
