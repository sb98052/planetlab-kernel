Summary: The Linux kernel (the core of the Linux operating system)

# What parts do we want to build?  We must build at least one kernel.
# These are the kernels that are built IF the architecture allows it.

%define buildup 1
%define buildsmp 1
%define builduml 1
%define buildxen 1
%define builddoc 1

# Versions of various parts

#
# Polite request for people who spin their own kernel rpms:
# please modify the "release" field in a way that identifies
# that the kernel isn't the stock distribution kernel, for example by
# adding some text to the end of the version number.
#
%define sublevel 18
%define kversion 2.6.%{sublevel}
%define rpmversion 2.6.%{sublevel}
%define release 1.2260_FC5.0%{?pldistro:.%{pldistro}}%{?date:.%{date}}
%define signmodules 0
%define make_target bzImage
%define kernel_arch i386

%define KVERREL %{PACKAGE_VERSION}-%{PACKAGE_RELEASE}

# Override generic defaults with per-arch defaults

%define image_install_path boot

#
# Three sets of minimum package version requirements in the form of Conflicts:
# to versions below the minimum
#

#
# First the general kernel 2.6 required versions as per
# Documentation/Changes
#
%define kernel_dot_org_conflicts  ppp <= 2.3.15, pcmcia-cs <= 3.1.20, isdn4k-utils <= 3.0, mount < 2.10r-5, nfs-utils < 1.0.3, e2fsprogs < 1.29, util-linux < 2.10, jfsutils < 1.0.14, reiserfsprogs < 3.6.3, xfsprogs < 2.1.0, procps < 2.0.9, oprofile < 0.5.3

# 
# Then a series of requirements that are distribution specific, either 
# because we add patches for something, or the older versions have 
# problems with the newer kernel or lack certain things that make 
# integration in the distro harder than needed.
#
%define package_conflicts  cipe < 1.4.5, kudzu <= 0.92, initscripts < 7.23, dev < 3.2-7, iptables < 1.2.5-3, bcm5820 < 1.81, nvidia-rh72 <= 1.0 selinux-policy-targeted < 1.23.16-1

#
# Several packages had bugs in them that became obvious when the NPTL
# threading code got integrated.
#
%define nptl_conflicts SysVinit < 2.84-13, pam < 0.75-48, vixie-cron < 3.0.1-73, privoxy < 3.0.0-8, spamassassin < 2.44-4.8.x,  cups < 1.1.17-13

#
# The ld.so.conf.d file we install uses syntax older ldconfig's don't grok.
#

# MEF commented out
# %define xen_conflicts glibc < 2.3.5-1

#
# Packages that need to be installed before the kernel is, because the %post
# scripts use them.
#
%define kernel_prereq  fileutils, module-init-tools, initscripts >= 5.83, mkinitrd >= 3.5.5

Vendor: PlanetLab
Packager: PlanetLab Central <support@planet-lab.org>
Distribution: PlanetLab 3.0
URL: http://cvs.planet-lab.org/cvs/linux-2.6

Name: kernel
Group: System Environment/Kernel
License: GPLv2
Version: %{rpmversion}
Release: %{release}
ExclusiveOS: Linux
Provides: kernel = %{version}
Provides: kernel-drm = 4.3.0
Provides: kernel-%{_target_cpu} = %{rpmversion}-%{release}
Prereq: %{kernel_prereq}
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
Conflicts: %{nptl_conflicts}
# We can't let RPM do the dependencies automatic because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel proper to function
AutoReqProv: no

#
# List the packages used during the kernel build
#
BuildPreReq: module-init-tools, patch >= 2.5.4, bash >= 2.03, sh-utils, tar
BuildPreReq: bzip2, findutils, gzip, m4, perl, make >= 3.78, gnupg, diffutils
BuildRequires: gcc >= 3.3.3, binutils >= 2.12, redhat-rpm-config
BuildConflicts: rhbuildsys(DiskFree) < 500Mb
#BuildArchitectures: i686


Source0: ftp://ftp.kernel.org/pub/linux/kernel/v2.6/linux-%{kversion}.tar.bz2

BuildRoot: %{_tmppath}/kernel-%{KVERREL}-root

%description
The kernel package contains the Linux kernel (vmlinuz), the core of any
Linux operating system.  The kernel handles the basic functions
of the operating system:  memory allocation, process allocation, device
input and output, etc.

%package devel
Summary: Development package for building kernel modules to match the kernel.
Group: System Environment/Kernel
AutoReqProv: no
Provides: kernel-devel-%{_target_cpu} = %{rpmversion}-%{release}
Prereq: /usr/bin/find

%description devel
This package provides kernel headers and makefiles sufficient to build modules
against the kernel package.


%package doc
Summary: Various documentation bits found in the kernel source.
Group: Documentation

%description doc
This package contains documentation files from the kernel
source. Various bits of information about the Linux kernel and the
device drivers shipped with it are documented in these files.

You'll want to install this package if you need a reference to the
options that can be passed to Linux kernel modules at load time.

%package smp
Summary: The Linux kernel compiled for SMP machines.

Group: System Environment/Kernel
Provides: kernel = %{version}
Provides: kernel-drm = 4.3.0
Provides: kernel-%{_target_cpu} = %{rpmversion}-%{release}smp
Prereq: %{kernel_prereq}
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
Conflicts: %{nptl_conflicts}
# upto and including kernel 2.4.9 rpms, the 4Gb+ kernel was called kernel-enterprise
# now that the smp kernel offers this capability, obsolete the old kernel
Obsoletes: kernel-enterprise < 2.4.10
# We can't let RPM do the dependencies automatic because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel proper to function
AutoReqProv: no

%description smp
This package includes a SMP version of the Linux kernel. It is
required only on machines with two or more CPUs as well as machines with
hyperthreading technology.

Install the kernel-smp package if your machine uses two or more CPUs.

%package smp-devel
Summary: Development package for building kernel modules to match the SMP kernel.
Group: System Environment/Kernel
Provides: kernel-smp-devel-%{_target_cpu} = %{rpmversion}-%{release}
Provides: kernel-devel-%{_target_cpu} = %{rpmversion}-%{release}smp
Provides: kernel-devel = %{rpmversion}-%{release}smp
AutoReqProv: no
Prereq: /usr/bin/find

%description smp-devel
This package provides kernel headers and makefiles sufficient to build modules
against the SMP kernel package.

%package xenU
Summary: The Linux kernel compiled for unprivileged Xen guest VMs

Group: System Environment/Kernel
Provides: kernel = %{version}
Provides: kernel-%{_target_cpu} = %{rpmversion}-%{release}xenU
Prereq: %{kernel_prereq}
Conflicts: %{kernel_dot_org_conflicts}
Conflicts: %{package_conflicts}
Conflicts: %{nptl_conflicts}

# MEF commented out 
# Conflicts: %{xen_conflicts}

# We can't let RPM do the dependencies automatic because it'll then pick up
# a correct but undesirable perl dependency from the module headers which
# isn't required for the kernel proper to function
AutoReqProv: no

%description xenU
This package includes a version of the Linux kernel which
runs in Xen unprivileged guest VMs.  This should be installed
both inside the unprivileged guest (for the modules) and in
the guest0 domain.

%package xenU-devel
Summary: Development package for building kernel modules to match the kernel.
Group: System Environment/Kernel
AutoReqProv: no
Provides: kernel-xenU-devel-%{_target_cpu} = %{rpmversion}-%{release}
Provides: kernel-devel-%{_target_cpu} = %{rpmversion}-%{release}xenU
Provides: kernel-devel = %{rpmversion}-%{release}xenU
Prereq: /usr/sbin/hardlink, /usr/bin/find

%description xenU-devel
This package provides kernel headers and makefiles sufficient to build modules
against the kernel package.

%package uml
Summary: The Linux kernel compiled for use in user mode (User Mode Linux).

Group: System Environment/Kernel

%description uml
This package includes a user mode version of the Linux kernel.

%package uml-devel
Summary: Development package for building kernel modules to match the UML kernel.
Group: System Environment/Kernel
Provides: kernel-uml-devel-%{_target_cpu} = %{rpmversion}-%{release}
Provides: kernel-devel-%{_target_cpu} = %{rpmversion}-%{release}smp
Provides: kernel-devel = %{rpmversion}-%{release}smp
AutoReqProv: no
Prereq: /usr/sbin/hardlink, /usr/bin/find

%description uml-devel
This package provides kernel headers and makefiles sufficient to build modules
against the User Mode Linux kernel package.

%package uml-modules
Summary: The Linux kernel modules compiled for use in user mode (User Mode Linux).

Group: System Environment/Kernel

%description uml-modules
This package includes a user mode version of the Linux kernel modules.

%package vserver
Summary: A placeholder RPM that provides kernel and kernel-drm

Group: System Environment/Kernel
Provides: kernel = %{version}
Provides: kernel-drm = 4.3.0
Provides: kernel-%{_target_cpu} = %{rpmversion}-%{release}

%description vserver
VServers do not require and cannot use kernels, but some RPMs have
implicit or explicit dependencies on the "kernel" package
(e.g. tcpdump). This package installs no files but provides the
necessary dependencies to make rpm and yum happy.


%prep
# First we unpack the kernel tarball.
# If this isn't the first make prep, we use links to the existing clean tarball
# which speeds things up quite a bit.
if [ ! -d kernel-%{kversion}/vanilla ]; then
  # Ok, first time we do a make prep.
  rm -f pax_global_header
%setup -q -n %{name}-%{version} -c
  mv linux-%{kversion} vanilla
else
  # We already have a vanilla dir.
  cd kernel-%{kversion}
fi

cd vanilla

# make sure the kernel has the sublevel we know it has. This looks weird
# but for -pre and -rc versions we need it since we only want to use
# the higher version when the final kernel is released.
perl -p -i -e "s/^SUBLEVEL.*/SUBLEVEL = %{sublevel}/" Makefile
perl -p -i -e "s/^EXTRAVERSION.*/EXTRAVERSION = -prep/" Makefile

# get rid of unwanted files resulting from patch fuzz
find . \( -name "*.orig" -o -name "*~" \) -exec rm -f {} \; >/dev/null


###
### build
###
%build

BuildKernel() {
    MakeTarget=$1
    Arch=$2
    Flavour=$3

    # create a clean copy in BUILD/ (for backward compatibility with
    # other RPMs that bootstrap off of the kernel build)
    cd $RPM_BUILD_DIR
    rm -rf linux-%{kversion}$Flavour
    cp -rl kernel-%{kversion}/vanilla linux-%{_target_cpu}-%{kversion}$Flavour
    cd linux-%{_target_cpu}-%{kversion}$Flavour

    # Pick the right config file for the kernel we're building
    if [ -n "$Flavour" ] ; then
      Config=kernel-%{kversion}-%{_target_cpu}-$Flavour-planetlab.config
      DevelDir=/usr/src/kernels/%{KVERREL}-$Flavour-%{_target_cpu}
      DevelLink=/usr/src/kernels/%{KVERREL}$Flavour-%{_target_cpu}
    else
      Config=kernel-%{kversion}-%{_target_cpu}-planetlab.config
      DevelDir=/usr/src/kernels/%{KVERREL}-%{_target_cpu}
      DevelLink=
    fi

    KernelVer=%{version}-%{release}$Flavour
    echo BUILDING A KERNEL FOR $Flavour %{_target_cpu}...

    # make sure EXTRAVERSION says what we want it to say
    perl -p -i -e "s/^EXTRAVERSION.*/EXTRAVERSION = -%{release}$Flavour/" Makefile

    # and now to start the build process

    make -s mrproper
    cp configs/$Config .config

    #Arch=`head -1 .config | cut -b 3-`
    echo USING ARCH=$Arch

    make -s ARCH=$Arch nonint_oldconfig > /dev/null
    make -s ARCH=$Arch %{?_smp_mflags} $MakeTarget
    make -s ARCH=$Arch %{?_smp_mflags} modules || exit 1

    # Start installing the results

%if "%{_enable_debug_packages}" == "1"
    mkdir -p $RPM_BUILD_ROOT/usr/lib/debug/boot
%endif
    mkdir -p $RPM_BUILD_ROOT/%{image_install_path}
    install -m 644 .config $RPM_BUILD_ROOT/boot/config-$KernelVer
    install -m 644 System.map $RPM_BUILD_ROOT/boot/System.map-$KernelVer
    if [ -f arch/$Arch/boot/bzImage ]; then
      cp arch/$Arch/boot/bzImage $RPM_BUILD_ROOT/%{image_install_path}/vmlinuz-$KernelVer
    fi
    if [ -f arch/$Arch/boot/zImage.stub ]; then
      cp arch/$Arch/boot/zImage.stub $RPM_BUILD_ROOT/%{image_install_path}/zImage.stub-$KernelVer || :
    fi
    if [ "$Flavour" = "uml" ] ; then
	install -D -m 755 linux $RPM_BUILD_ROOT/%{_bindir}/linux
    fi

    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer
    make -s ARCH=$Arch INSTALL_MOD_PATH=$RPM_BUILD_ROOT modules_install KERNELRELEASE=$KernelVer

    # And save the headers/makefiles etc for building modules against
    #
    # This all looks scary, but the end result is supposed to be:
    # * all arch relevant include/ files
    # * all Makefile/Kconfig files
    # * all script/ files

    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/source
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    (cd $RPM_BUILD_ROOT/lib/modules/$KernelVer ; ln -s build source)
    # dirs for additional modules per module-init-tools, kbuild/modules.txt
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/extra
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/updates
    # first copy everything
    cp --parents `find  -type f -name "Makefile*" -o -name "Kconfig*"` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build 
	cp Module.symvers $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    if [ "$Flavour" = "uml" ] ; then
      cp --parents -a `find arch/um -name include` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    fi
    # then drop all but the needed Makefiles/Kconfig files
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Documentation
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts
    rm -rf $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cp arch/%{_arch}/kernel/asm-offsets.s $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch}/kernel || :
    cp .config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    cp -a scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    if [ -d arch/%{_arch}/scripts ]; then
      cp -a arch/%{_arch}/scripts $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch} || :
    fi
    if [ -f arch/%{_arch}/*lds ]; then
      cp -a arch/%{_arch}/*lds $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/%{_arch}/ || :
    fi
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*.o
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/scripts/*/*.o
    mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cd include
    cp -a acpi config keys linux math-emu media mtd net pcmcia rdma rxrpc scsi sound video asm asm-generic $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    cp -a `readlink asm` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    if [ "$Arch" = "x86_64" ]; then
      cp -a asm-i386 $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    fi
%if %{buildxen}
    if [ "$Flavour" = "xenU" ]; then
      cp -a xen $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
      cp -a asm-i386 $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    fi
%endif
%if %{builduml}
    if [ "$Flavour" = "uml" ] ; then
      cp -a `readlink -f asm/arch` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
    fi
%endif
    # While arch/powerpc/include/asm is still a symlink to the old
    # include/asm-ppc{64,} directory, include that in kernel-devel too.
    if [ "$Arch" = "powerpc" -a -r ../arch/powerpc/include/asm ]; then
      cp -a `readlink ../arch/powerpc/include/asm` $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include
      mkdir -p $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/$Arch/include
      pushd $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/arch/$Arch/include
      ln -sf ../../../include/asm-ppc* asm
      popd
    fi

    # Make sure the Makefile and version.h have a matching timestamp so that
    # external modules can be built
    touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/Makefile $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/version.h
    touch -r $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/.config $RPM_BUILD_ROOT/lib/modules/$KernelVer/build/include/linux/autoconf.h
    cd ..

    #
    # save the vmlinux file for kernel debugging into the kernel-debuginfo rpm
    #
%if "%{_enable_debug_packages}" == "1"
    mkdir -p $RPM_BUILD_ROOT/usr/lib/debug/lib/modules/$KernelVer
    cp vmlinux $RPM_BUILD_ROOT/usr/lib/debug/lib/modules/$KernelVer
%endif
%if %{buildxen}
    if [ -f vmlinuz ]; then
      cp vmlinuz $RPM_BUILD_ROOT/%{image_install_path}/vmlinuz-$KernelVer
    fi
%endif

    find $RPM_BUILD_ROOT/lib/modules/$KernelVer -name "*.ko" -type f >modnames

    # mark modules executable so that strip-to-file can strip them
    cat modnames | xargs chmod u+x

    # remove files that will be auto generated by depmod at rpm -i time
    rm -f $RPM_BUILD_ROOT/lib/modules/$KernelVer/modules.*

    # Move the devel headers out of the root file system
    mkdir -p $RPM_BUILD_ROOT/usr/src/kernels
    mv $RPM_BUILD_ROOT/lib/modules/$KernelVer/build $RPM_BUILD_ROOT/$DevelDir
    ln -sf ../../..$DevelDir $RPM_BUILD_ROOT/lib/modules/$KernelVer/build
    [ -z "$DevelLink" ] || ln -sf `basename $DevelDir` $RPM_BUILD_ROOT/$DevelLink
}

###
# DO it...
###

# prepare directories
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/boot

%if %{buildup}
BuildKernel %make_target %kernel_arch
%endif

%if %{buildsmp} && "%{_target_cpu}" == "i686"
BuildKernel %make_target %kernel_arch smp
%endif

%if %{builduml} && "%{_target_cpu}" == "i686"
BuildKernel linux um uml
%endif

%if %{buildxen} && "%{_target_cpu}" == "i686"
BuildKernel vmlinuz %kernel_arch xenU
%endif

###
### install
###

%install

cd vanilla

%if %{buildxen} && "%{_target_cpu}" == "i686"
mkdir -p $RPM_BUILD_ROOT/etc/ld.so.conf.d
rm -f $RPM_BUILD_ROOT/etc/ld.so.conf.d/kernelcap-%{KVERREL}.conf
cat > $RPM_BUILD_ROOT/etc/ld.so.conf.d/kernelcap-%{KVERREL}.conf <<\EOF
# This directive teaches ldconfig to search in nosegneg subdirectories
# and cache the DSOs there with extra bit 0 set in their hwcap match
# fields.  In Xen guest kernels, the vDSO tells the dynamic linker to
# search in nosegneg subdirectories and to match this extra hwcap bit
# in the ld.so.cache file.
hwcap 0 nosegneg
EOF
chmod 444 $RPM_BUILD_ROOT/etc/ld.so.conf.d/kernelcap-%{KVERREL}.conf
%endif

%if %{builddoc}
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/kernel-doc-%{kversion}/Documentation

# sometimes non-world-readable files sneak into the kernel source tree
chmod -R a+r *
# copy the source over
tar cf - Documentation | tar xf - -C $RPM_BUILD_ROOT/usr/share/doc/kernel-doc-%{kversion}
%endif

###
### clean
###

%clean
rm -rf $RPM_BUILD_ROOT

###
### scripts
###

# load the loop module for upgrades...in case the old modules get removed we have
# loopback in the kernel so that mkinitrd will work.
%pre
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
exit 0

%pre smp
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
exit 0

%post
if [ `uname -i` == "x86_64" ]; then
  if [ -f /etc/sysconfig/kernel ]; then
    /bin/sed -i -e 's/^DEFAULTKERNEL=kernel-smp$/DEFAULTKERNEL=kernel/' /etc/sysconfig/kernel
  fi
fi

# trick mkinitrd in case the current environment does not have device mapper
rootdev=$(awk '/^[ \t]*[^#]/ { if ($2 == "/") { print $1; }}' /etc/fstab)
if echo $rootdev |grep -q /dev/mapper 2>/dev/null ; then
    if [ ! -f $rootdev ]; then
	fake_root_lvm=1
	mkdir -p $(dirname $rootdev)
	touch $rootdev
    fi
fi

[ ! -x /usr/sbin/module_upgrade ] || /usr/sbin/module_upgrade %{rpmversion}-%{release}
#/sbin/new-kernel-pkg --package kernel --mkinitrd --depmod --install %{KVERREL}
# Older modutils do not support --package option
/sbin/new-kernel-pkg --mkinitrd --depmod --install %{KVERREL}

# remove fake handle
if [ -n "$fake_root_lvm" ]; then
    rm -f $rootdev
fi

# make some useful links
pushd /boot > /dev/null ; {
	ln -sf config-%{KVERREL} config
	ln -sf initrd-%{KVERREL}.img initrd-boot
	ln -sf vmlinuz-%{KVERREL} kernel-boot
}
popd > /dev/null

# ask for a reboot
mkdir -p /etc/planetlab
touch /etc/planetlab/update-reboot

%post devel
[ -f /etc/sysconfig/kernel ] && . /etc/sysconfig/kernel
if [ "$HARDLINK" != "no" -a -x /usr/sbin/hardlink ] ; then
  pushd /usr/src/kernels/%{KVERREL}-%{_target_cpu} > /dev/null
  /usr/bin/find . -type f | while read f; do hardlink -c /usr/src/kernels/*FC*/$f $f ; done
  popd > /dev/null
fi

%post smp
# trick mkinitrd in case the current environment does not have device mapper
rootdev=$(awk '/^[ \t]*[^#]/ { if ($2 == "/") { print $1; }}' /etc/fstab)
if echo $rootdev |grep -q /dev/mapper 2>/dev/null ; then
    if [ ! -f $rootdev ]; then
	fake_root_lvm=1
	mkdir -p $(dirname $rootdev)
	touch $rootdev
    fi
fi

[ ! -x /usr/sbin/module_upgrade ] || /usr/sbin/module_upgrade %{rpmversion}-%{release}smp
#/sbin/new-kernel-pkg --package kernel-smp --mkinitrd --depmod --install %{KVERREL}smp
# Older modutils do not support --package option
/sbin/new-kernel-pkg --mkinitrd --depmod --install %{KVERREL}smp

# remove fake handle
if [ -n "$fake_root_lvm" ]; then
    rm -f $rootdev
fi

# make some useful links
pushd /boot > /dev/null ; {
	ln -sf config-%{KVERREL}smp configsmp
	ln -sf initrd-%{KVERREL}smp.img initrd-bootsmp
	ln -sf vmlinuz-%{KVERREL}smp kernel-bootsmp
}
popd > /dev/null

# ask for a reboot
mkdir -p /etc/planetlab
touch /etc/planetlab/update-reboot

%post smp-devel
[ -f /etc/sysconfig/kernel ] && . /etc/sysconfig/kernel
if [ "$HARDLINK" != "no" -a -x /usr/sbin/hardlink ] ; then
  pushd /usr/src/kernels/%{KVERREL}-smp-%{_target_cpu} > /dev/null
  /usr/bin/find . -type f | while read f; do hardlink -c /usr/src/kernels/*FC*/$f $f ; done
  popd > /dev/null
fi

%post xenU
[ ! -x /usr/sbin/module_upgrade ] || /usr/sbin/module_upgrade
[ ! -x /sbin/ldconfig ] || /sbin/ldconfig -X

%post xenU-devel
[ -f /etc/sysconfig/kernel ] && . /etc/sysconfig/kernel
if [ "$HARDLINK" != "no" -a -x /usr/sbin/hardlink ] ; then
  pushd /usr/src/kernels/%{KVERREL}-xenU-%{_target_cpu} > /dev/null
  /usr/bin/find . -type f | while read f; do hardlink -c /usr/src/kernels/*FC*/$f $f ; done
  popd > /dev/null
fi

%post uml-modules
depmod -ae %{KVERREL}uml

%preun
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
/sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}

%preun smp
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
/sbin/new-kernel-pkg --rminitrd --rmmoddep --remove %{KVERREL}smp

%preun xenU
/sbin/modprobe loop 2> /dev/null > /dev/null  || :
/sbin/new-kernel-pkg --rmmoddep --remove %{KVERREL}xenU

%preun uml-modules
rm -f /lib/modules/%{KVERREL}uml/modules.*


###
### file lists
###

%if %{buildup}
%files 
%defattr(-,root,root)
/%{image_install_path}/vmlinuz-%{KVERREL}
/boot/System.map-%{KVERREL}
/boot/config-%{KVERREL}
%dir /lib/modules/%{KVERREL}
/lib/modules/%{KVERREL}/kernel
/lib/modules/%{KVERREL}/build
/lib/modules/%{KVERREL}/source
/lib/modules/%{KVERREL}/extra
/lib/modules/%{KVERREL}/updates

%files devel
%defattr(-,root,root)
%verify(not mtime) /usr/src/kernels/%{KVERREL}-%{_target_cpu}
%endif

%if %{buildsmp} && "%{_target_cpu}" == "i686"
%files smp
%defattr(-,root,root)
/%{image_install_path}/vmlinuz-%{KVERREL}smp
/boot/System.map-%{KVERREL}smp
/boot/config-%{KVERREL}smp
%dir /lib/modules/%{KVERREL}smp
/lib/modules/%{KVERREL}smp/kernel
/lib/modules/%{KVERREL}smp/build
/lib/modules/%{KVERREL}smp/source
/lib/modules/%{KVERREL}smp/extra
/lib/modules/%{KVERREL}smp/updates

%files smp-devel
%defattr(-,root,root)
%verify(not mtime) /usr/src/kernels/%{KVERREL}-smp-%{_target_cpu}
/usr/src/kernels/%{KVERREL}smp-%{_target_cpu}
%endif

%if %{builduml} && "%{_target_cpu}" == "i686"
%files uml
%defattr(-,root,root)
%{_bindir}/linux

%files uml-devel
%defattr(-,root,root)
%verify(not mtime) /usr/src/kernels/%{KVERREL}-uml-%{_target_cpu}
/usr/src/kernels/%{KVERREL}uml-%{_target_cpu}

%files uml-modules
%defattr(-,root,root)
/boot/System.map-%{KVERREL}uml
/boot/config-%{KVERREL}uml
%dir /lib/modules/%{KVERREL}uml
/lib/modules/%{KVERREL}uml/kernel
/lib/modules/%{KVERREL}uml/build
/lib/modules/%{KVERREL}uml/source
/lib/modules/%{KVERREL}uml/extra
/lib/modules/%{KVERREL}uml/updates
%endif

%if %{buildxen} && "%{_target_cpu}" == "i686"
%files xenU
%defattr(-,root,root)
/%{image_install_path}/vmlinuz-%{KVERREL}xenU
/boot/System.map-%{KVERREL}xenU
/boot/config-%{KVERREL}xenU
%dir /lib/modules/%{KVERREL}xenU
/lib/modules/%{KVERREL}xenU/kernel
/lib/modules/%{KVERREL}xenU/build
/lib/modules/%{KVERREL}xenU/source
/lib/modules/%{KVERREL}xenU/extra
/lib/modules/%{KVERREL}xenU/updates
/etc/ld.so.conf.d/kernelcap-%{KVERREL}.conf

%files xenU-devel
%defattr(-,root,root)
%verify(not mtime) /usr/src/kernels/%{KVERREL}-xenU-%{_target_cpu}
/usr/src/kernels/%{KVERREL}xenU-%{_target_cpu}
%endif

%files vserver
%defattr(-,root,root)
# no files

# only some architecture builds need kernel-doc

%if %{builddoc}
%files doc
%defattr(-,root,root)
%{_datadir}/doc/kernel-doc-%{kversion}/Documentation/*
%dir %{_datadir}/doc/kernel-doc-%{kversion}/Documentation
%dir %{_datadir}/doc/kernel-doc-%{kversion}
%endif

%changelog
* Tue Jul 11 2006 Dave Jones <davej@redhat.com> [2.6.17-1.2142_FC4]
- 2.6.17.4
- Disable split pagetable lock.

* Wed Jul  5 2006 Dave Jones <davej@redhat.com>
- Get rid of stack backtrace on panic, which in most
  cases actually caused a loss of info instead of a gain.

* Fri Jun 30 2006 Dave Jones <davej@redhat.com> [2.6.17-1.2141_FC4]
- 2.6.17.3

* Fri Jun 30 2006 Dave Jones <davej@redhat.com> [2.6.17-1.2140_FC4]
- 2.6.17.2
- Fix up the alsa list_add bug.

* Mon Jun 26 2006 Dave Jones <davej@redhat.com>
- Fix up various stupidities incurred by the last big rebase.
  - Reenable SMP x86-64 builds.
  - Reenable SMBFS.
- Enable PCI fake hotplug driver.
- Enable gameport/joystick on i586 builds. (#196581)

* Sat Jun 24 2006 Dave Jones <davej@redhat.com>
- Enable profiling for 586 kernels.

* Fri Jun 23 2006 Dave Jones <davej@redhat.com> [2.6.17-1.2139_FC4]
- Rebuild with slab debug off.

* Tue Jun 20 2006 Dave Jones <davej@redhat.com> [2.6.17-1.2138_FC4]
- 2.6.17.1

* Mon Jun  5 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2115_FC4]
- 2.6.16.20

* Tue May 30 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2114_FC4]
- 2.6.16.19

* Mon May 29 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2113_FC4]
- Improved list_head debugging.

* Tue May 23 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2112_FC4]
- 2.6.16.18

* Sat May 20 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2111_FC4]
- 2.6.16.17

* Wed May 10 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2110_FC4]
- 2.6.16.16

* Tue May  9 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2109_FC4]
- 2.6.16.15
- make 16C950 UARTs work (again). (#126403)
- Fix exec-shield default, which should fix a few programs that
  stopped running.

* Thu May  4 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2108_FC4]
- 2.6.16.14

* Tue May  2 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2107_FC4]
- 2.6.16.13

* Mon May  1 2006 Dave Jones <davej@redhat.com>
- 2.6.16.12

* Tue Apr 25 2006 Dave Jones <davej@redhat.com>
- Fix up SCSI errors with mymusix usb mp3 player (#186187)

* Mon Apr 24 2006 Dave Jones <davej@redhat.com>
- 2.6.16.11

* Wed Apr 19 2006 Dave Jones <davej@redhat.com>
- Enable PCI MSI support.

* Tue Apr 18 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2096_FC4]
- 2.6.16.9

* Tue Apr 18 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2093_FC4]
- 2.6.16.7

* Mon Apr 17 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2092_FC4]
- 2.6.16.6

* Sun Apr 16 2006 Dave Jones <davej@redhat.com>
- 2.6.16.5

* Fri Apr  7 2006 Dave Jones <davej@redhat.com>
- 2.6.16.2

* Tue Apr  4 2006 Dave Jones <davej@redhat.com>
- Reenable non-standard serial ports. (#187466)
- Reenable snd-es18xx for x86-32 (#187733)

* Thu Mar 30 2006 Dave Jones <davej@redhat.com>
- ship the .kernelrelease file in -devel too.
- Disable EDAC debug.

* Tue Mar 26 2006 Dave Jones <davej@redhat.com> [2.6.16-1.2069_FC4]
- 2.6.16.1

* Mon Mar 25 2006 Dave Jones <davej@redhat.com>
- Include patches posted for review for inclusion in 2.6.16.1
- Updated new audit msg types.
- Reenable HDLC driver (#186257)
- Make acpi-cpufreq 'sticky'
- Fix broken x86-64 32bit vDSO (#186924)

* Tue Mar 21 2006 Dave Jones <davej@redhat.com>
- Improve spinlock scalability on big machines.

* Mon Mar 20 2006 Dave Jones <davej@redhat.com>
- Sync with FC5's 2.6.16 kernel.
- Update Tux & Exec-shield to latest.

