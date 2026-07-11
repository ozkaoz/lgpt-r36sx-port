#!/bin/sh
BASE=/mnt/sdcard/lgpt/otg
MODBASE=$BASE/modules/4.4.186-release
LOGDIR=$BASE/logs
mkdir -p "$LOGDIR" 2>/dev/null || true

load_first_existing_au11() {
  name="$1"
  modname="${name%.ko}"
  modname="$(echo "$modname" | tr '-' '_')"
  if grep -q "^$modname " /proc/modules 2>/dev/null; then echo "LOAD_${name}_ALREADY=YES"; return 0; fi
  for d in \
    "$MODBASE/u2_38am_q2_alsa" \
    "$MODBASE/u2_38q2" \
    "$MODBASE/u2_38y_q2_alsa" \
    "$MODBASE/u2_38ao_q2_dummy_uac1" \
    "$MODBASE/u2_38ak_hybrid_uac1" \
    $(find "$MODBASE" -maxdepth 2 -type d 2>/dev/null); do
    if [ -f "$d/$name" ]; then
      insmod "$d/$name" 2>/tmp/au11_insmod.err
      rc=$?
      echo "LOAD_${name}_RC=$rc FROM=$d ERR=$(cat /tmp/au11_insmod.err 2>/dev/null)"
      return $rc
    fi
  done
  echo "LOAD_${name}_MISSING=YES"
  return 2
}


restore_modules_from_backup_runtime_au11v2() {
  if find "$BASE/modules" -type f -name 'soundcore.ko' 2>/dev/null | grep -q .; then
    echo "AU11Z_RUNTIME_MODULES_PRESENT=YES"
    return 0
  fi
  for cand in $(find /mnt/sdcard -path '*/lgpt/otg/modules' -type d 2>/dev/null | sort -r); do
    [ "$cand" = "$BASE/modules" ] && continue
    if find "$cand" -type f -name 'soundcore.ko' 2>/dev/null | grep -q .; then
      echo "AU11Z_RUNTIME_RESTORE_MODULES_FROM=$cand"
      mkdir -p "$BASE" 2>/dev/null || true
      cp -a "$cand" "$BASE/modules" 2>/dev/null || echo "WARN_AU11Z_RUNTIME_MODULE_COPY_FAILED=YES"
      return 0
    fi
  done
  echo "WARN_AU11Z_RUNTIME_NO_MODULES_FOUND=YES"
  return 0
}

load_audio_stack_au11() {
  variant="${1:-sync}"
  restore_modules_from_backup_runtime_au11v2
  AU8_MODDIR=$MODBASE/u2_38au8_${variant}_uac2
  load_first_existing_au11 soundcore.ko || return $?
  load_first_existing_au11 snd.ko || return $?
  load_first_existing_au11 snd-timer.ko || return $?
  load_first_existing_au11 snd-pcm.ko || return $?
  if grep -q '^libcomposite ' /proc/modules 2>/dev/null; then
    echo "LOAD_libcomposite_ALREADY=YES"
  else
    for p in /lib/modules/4.4.186-release/kernel/drivers/usb/gadget/libcomposite.ko /lib32/modules/4.4.186-release/kernel/drivers/usb/gadget/libcomposite.ko "$MODBASE/stock/libcomposite.ko"; do
      [ -f "$p" ] || continue
      insmod "$p" 2>/tmp/au11_lc.err
      rc=$?
      echo "LOAD_libcomposite_RC=$rc FROM=$p ERR=$(cat /tmp/au11_lc.err 2>/dev/null)"
      [ $rc -eq 0 ] || return $rc
      break
    done
  fi
  if grep -q '^usb_f_uac2 ' /proc/modules 2>/dev/null; then
    echo "LOAD_usb_f_uac2_ALREADY=YES"
    return 0
  fi
  if [ -f "$AU8_MODDIR/usb_f_uac2.ko" ]; then
    insmod "$AU8_MODDIR/usb_f_uac2.ko" 2>/tmp/au11_uac2.err
    rc=$?
    echo "LOAD_usb_f_uac2_RC=$rc FROM=$AU8_MODDIR ERR=$(cat /tmp/au11_uac2.err 2>/dev/null)"
    return $rc
  fi
  echo "LOAD_usb_f_uac2_MISSING=$AU8_MODDIR/usb_f_uac2.ko"
  return 2
}

print_udc_status_au11() {
  echo "[UDC status AU11]"
  ls -l /sys/class/udc 2>/dev/null || true
  for u in /sys/class/udc/*; do
    [ -e "$u/state" ] || continue
    echo "UDC=$(basename "$u") STATE=$(cat "$u/state" 2>/dev/null) SPEED=$(cat "$u/current_speed" 2>/dev/null)"
  done
}

switch_roles_peripheral_au11() {
  echo "[role switch AU11: write peripheral]"
  find /sys/devices -path '*musb-hdrc*.auto/mode' 2>/dev/null | sort | while read p; do
    echo "MODE_BEFORE $p=$(cat "$p" 2>/dev/null)"
    echo peripheral > "$p" 2>/tmp/au11_mode.err
    rc=$?
    echo "MODE_WRITE_PERIPHERAL_RC=$rc PATH=$p ERR=$(cat /tmp/au11_mode.err 2>/dev/null)"
    echo "MODE_AFTER_PERIPHERAL $p=$(cat "$p" 2>/dev/null)"
  done
  print_udc_status_au11
}

cleanup_gadgets_au11() {
  echo "[AU11 cleanup: r36sx/lgpt/u2_38 UAC gadgets]"
  mount -t configfs none /sys/kernel/config 2>/dev/null || true
  for g in /sys/kernel/config/usb_gadget/r36sx* /sys/kernel/config/usb_gadget/lgpt* /sys/kernel/config/usb_gadget/u2_38*; do
    [ -d "$g" ] || continue
    echo "CLEANUP_GADGET=$g"
    echo "" > "$g/UDC" 2>/dev/null || true
  done
  sleep 1
  for g in /sys/kernel/config/usb_gadget/r36sx* /sys/kernel/config/usb_gadget/lgpt* /sys/kernel/config/usb_gadget/u2_38*; do
    [ -d "$g" ] || continue
    find "$g/configs" -type l -exec rm -f {} \; 2>/dev/null || true
    find "$g/functions" -maxdepth 1 -mindepth 1 -type d -exec rmdir {} \; 2>/dev/null || true
    find "$g/configs" -depth -type d -exec rmdir {} \; 2>/dev/null || true
    find "$g/strings" -depth -type d -exec rmdir {} \; 2>/dev/null || true
    rmdir "$g" 2>/dev/null || true
  done
}

bind_prefer_udc_au11() {
  G="$1"; PREFERRED="$2"; tried=""
  for udc in $PREFERRED musb-hdrc.0.auto musb-hdrc.1.auto $(ls /sys/class/udc 2>/dev/null); do
    [ -n "$udc" ] || continue
    case " $tried " in *" $udc "*) continue;; esac
    tried="$tried $udc"
    [ -e "/sys/class/udc/$udc" ] || continue
    echo "BIND_TRY_UDC=$udc"
    echo "$udc" > "$G/UDC" 2>/tmp/au11_bind.err
    rc=$?
    echo "BIND_RC=$rc UDC=$udc ERR=$(cat /tmp/au11_bind.err 2>/dev/null)"
    sleep 2
    print_udc_status_au11
    [ "$rc" = 0 ] && return 0
  done
  return 1
}

create_uac2_legacy_duplex_gadget_au11() {
  PREFERRED_UDC="${1:-musb-hdrc.0.auto}"
  PROFILE="duplex_stable_always_open"
  P_CHMASK=1
  C_CHMASK=1
  CONFIG_TEXT="U2.38AU11Z stable AU11U core + AU10Y known-good duplex descriptor"
  SERIAL="R36SX-U2-38AU10Y-DUPLEX"
  mount -t configfs none /sys/kernel/config 2>/dev/null || true
  cleanup_gadgets_au11
  switch_roles_peripheral_au11
  G="/sys/kernel/config/usb_gadget/r36sx_uac2_au11_duplex"
  mkdir -p "$G" || return 1
  cd "$G" || return 1
  echo 0x1209 > idVendor
  echo 0x38EA > idProduct
  echo 0x0102 > bcdDevice
  echo 0x0200 > bcdUSB
  echo 0x01 > bDeviceClass
  echo 0x01 > bDeviceSubClass
  echo 0x20 > bDeviceProtocol
  mkdir -p strings/0x409 configs/c.1/strings/0x409
  echo "$SERIAL" > strings/0x409/serialnumber
  echo "R36SX" > strings/0x409/manufacturer
  echo "R36SX USB Audio" > strings/0x409/product
  echo "$CONFIG_TEXT" > configs/c.1/strings/0x409/configuration
  echo 120 > configs/c.1/MaxPower
  mkdir -p functions/uac2.usb0
  echo "$P_CHMASK" > functions/uac2.usb0/p_chmask
  echo 48000 > functions/uac2.usb0/p_srate
  echo 2 > functions/uac2.usb0/p_ssize
  echo "$C_CHMASK" > functions/uac2.usb0/c_chmask
  echo 48000 > functions/uac2.usb0/c_srate
  echo 2 > functions/uac2.usb0/c_ssize
  echo "[uac2 attrs AU11Z_AU10Y_DESCRIPTOR duplex stable always-open profile=$PROFILE]"
  for a in p_chmask p_srate p_ssize c_chmask c_srate c_ssize; do echo "$a=$(cat functions/uac2.usb0/$a 2>/dev/null)"; done
  ln -s functions/uac2.usb0 configs/c.1/uac2.usb0
  echo "$PROFILE" > /tmp/r36sx_au11_active_profile 2>/dev/null || true
  echo "$PROFILE" > "$BASE/au11_active_usb_profile" 2>/dev/null || true
  bind_prefer_udc_au11 "$G" "$PREFERRED_UDC"
  return $?
}
