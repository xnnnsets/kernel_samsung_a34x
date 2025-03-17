#! /usr/bin/env bash

#
# Rissu Kernel Project
# A special build script for Rissu's kernel
#

# << If unset, you can override if u want
[ -z $IS_CI ] && IS_CI=false
[ -z $DO_CLEAN ] && DO_CLEAN=false
[ -z $LTO ] && LTO=thin
[ -z $DEFAULT_KSUN_REPO ] && DEFAULT_KSUN_REPO="https://raw.githubusercontent.com/KernelSU-Next/KernelSU-Next/refs/heads/next-susfs-dev/kernel/setup.sh"
[ -z $DEFAULT_KSU_REPO ] && DEFAULT_KSU_REPO="https://raw.githubusercontent.com/rsuntk/KernelSU/refs/heads/main/kernel/setup.sh"
[ -z $DEFAULT_AK3_REPO ] && DEFAULT_AK3_REPO="https://github.com/rsuntk/AnyKernel3.git"
[ -z $DEVICE ] && DEVICE="A346E"

# special rissu's path. linked to his toolchains
if [ -d /rsuntk ]; then
	export CROSS_COMPILE=/rsuntk/toolchains/gcc-10/bin/aarch64-buildroot-linux-gnu-
	export PATH=/rsuntk/toolchains/clang-20/bin:$PATH
fi

# start of default args
DEFAULT_ARGS="
CONFIG_SECTION_MISMATCH_WARN_ONLY=y
ARCH=arm64
KCFLAGS=-w
"
export ARCH=arm64
export CLANG_TRIPLE=aarch64-linux-gnu-
# end of default args

strip() { # fmt: strip <module>
	llvm-strip $@ --strip-unneeded
}
setconfig() { # fmt: setconfig enable/disable <NAME>
	[ -e $(pwd)/.config ] && config_file="$(pwd)/.config" || config_file="$(pwd)/out/.config"
	if [ -d $(pwd)/scripts ]; then
		./scripts/config --file `echo $config_file` --`echo $1` CONFIG_`echo $2`
	else
		echo "! Folder scripts not found!"
		exit
	fi
}
clone_ak3() {
	[ ! -d $(pwd)/AnyKernel3 ] && git clone `echo $DEFAULT_AK3_REPO` --depth=1 AnyKernel3 && rm -rf AnyKernel3/.git
}
gen_getutsrelease() {
# generate simple c file
if [ ! -e utsrelease.c ]; then
echo "/* Generated file by `basename $0` */
#include <stdio.h>
#ifdef __OUT__
#include \"out/include/generated/utsrelease.h\"
#else
#include \"include/generated/utsrelease.h\"
#endif

char utsrelease[] = UTS_RELEASE;

int main() {
	printf(\"%s\n\", utsrelease);
	return 0;
}" > utsrelease.c
fi
}
pr_invalid() {
	echo -e "[-] Invalid args: $@"
	exit
}
pr_err() {
	echo -e "[-] $@"
	exit
}
pr_info() {
	echo -e "[+] $@"
}
usage() {
	echo -e "Usage: bash `basename $0` <build_target> <-j | --jobs> <(job_count)> <defconfig>"
	printf "\tbuild_target: dirty, kernel, config, clean\n"
	printf "\t-j or --jobs: <int>\n"
	
	[ -d arch/$ARCH/configs ] && printf "\tavailable defconfig: `ls arch/arm64/configs`\n"
	
	echo ""
	printf "NOTE: Run: \texport CROSS_COMPILE=\"<PATH_TO_ANDROID_CC>\"\n"
	printf "\t\texport PATH=\"<PATH_TO_LLVM>\"\n"
	printf "before running this script!\n"
	printf "\n"
	printf "Misc:\n"
	printf "\tPOST_BUILD_CLEAN: Clean post build: (opt:boolean)\n"
	printf "\tLTO: Use Link-time Optimization; options: (opt: none, thin, full)\n"
	exit;
}

# if first arg starts with "clean"
if [[ "$1" = "clean" ]]; then
	[ $# -gt 1 ] && pr_err "Excess argument, only need one argument."
 	if [ -d $(pwd)/out ] || [ -d $(pwd)/.config ]; then
  		pr_info "Cleaning dirs .."
    	else
     		pr_err "No need clean"
 	fi
  
	if [ -d $(pwd)/out ]; then
		rm -rf out
	elif [ -f $(pwd)/.config ]; then
		make clean && make mrproper
	fi
	pr_err "All clean."
elif [[ "$1" = "dirty" ]]; then
	[ $# -gt 3 ] && pr_err "Excess argument, only need three argument."
	pr_info "Starting dirty build"
	FIRST_JOB="$2"
	JOB_COUNT="$3"
	if [ "$FIRST_JOB" = "-j" ] || [ "$FIRST_JOB" = "--jobs" ]; then
		if [ ! -z $JOB_COUNT ]; then
			ALLOC_JOB=$JOB_COUNT
		else
			pr_invalid $3
		fi
	else
		pr_invalid $2
	fi
	make -j`echo $ALLOC_JOB` -C $(pwd) O=$(pwd)/out `echo $DEFAULT_ARGS`
 	pr_err "Dirty build done. Clean build is recommended."
elif [[ "$1" = "ak3" ]]; then
	if [ $# -gt 1 ]; then
		pr_err "Excess argument, only need one argument."
	fi
	clone_ak3;
else
	[ $# != 4 ] && usage;
fi

[ "$KERNELSU_NEXT_SUSFS" = "true" ] && curl -LSs $DEFAULT_KSUN_REPO | bash -s next-susfs-dev || pr_info "KernelSU-Next-Susfs is disabled. Add 'KERNELSU_NEXT_SUSFS=true' or 'export KERNELSU_NEXT_SUSFS=true' to enable"
[ "$KERNELSU_NEXT" = "true" ] && curl -LSs $DEFAULT_KSUN_REPO | bash -s main || pr_info "KernelSU is disabled. Add 'KERNELSU_NEXT=true' or 'export KERNELSU_NEXT=true' to enable"
[ "$KERNELSU" = "true" ] && curl -LSs $DEFAULT_KSU_REPO | bash -s next-susfs-dev || pr_info "KernelSU is disabled. Add 'KERNELSU=true' or 'export KERNELSU=true' to enable"

BUILD_TARGET="$1"
FIRST_JOB="$2"
JOB_COUNT="$3"
DEFCONFIG="$4"

if [ "$BUILD_TARGET" = "kernel" ]; then
	BUILD="kernel"
elif [ "$BUILD_TARGET" = "defconfig" ]; then
	BUILD="defconfig"
else
	pr_invalid $1
fi

if [ "$FIRST_JOB" = "-j" ] || [ "$FIRST_JOB" = "--jobs" ]; then
	if [ ! -z $JOB_COUNT ]; then
		ALLOC_JOB=$JOB_COUNT
	else
		pr_invalid $3
	fi
else
	pr_invalid $2
fi

if [ ! -z "$DEFCONFIG" ]; then
	BUILD_DEFCONFIG="$DEFCONFIG"
else
	pr_invalid $4
fi

IMAGE="$(pwd)/out/arch/arm64/boot/Image"

pr_sum() {
	[ -z $KBUILD_BUILD_USER ] && KBUILD_BUILD_USER="`whoami`"
	[ -z $KBUILD_BUILD_HOST ] && KBUILD_BUILD_HOST="`hostname`"
	
	echo ""
	echo -e "Host Arch: `uname -m`"
	echo -e "Host Kernel: `uname -r`"
	echo -e "Host gnumake: `make -v | grep -e "GNU Make"`"
	echo ""
	echo -e "Linux version: `make kernelversion`"
	echo -e "Kernel builder user: $KBUILD_BUILD_USER"
	echo -e "Kernel builder host: $KBUILD_BUILD_HOST"
	echo -e "Build date: `date`"
	echo -e "Build target: `echo $BUILD`"
	echo -e "Arch: $ARCH"
	echo -e "Defconfig: $BUILD_DEFCONFIG"
	echo -e "Allocated core: $ALLOC_JOB"
	echo ""
	echo -e "LTO: $LTO"
	echo ""
}

pr_post_build() {
	echo ""
	echo -e "## Build $@ at `date` ##"
	echo ""
	[ "$@" = "failed" ] && exit
}

post_build_clean() {
	if [ -e $AK3 ]; then
		rm -rf $AK3/Image
		rm -rf $AK3/modules/vendor/lib/modules/*.ko
		#sed -i "s/do\.modules=.*/do.modules=0/" "$(pwd)/AnyKernel3/anykernel.sh"
		echo "stub" > $AK3/modules/vendor/lib/modules/stub
	fi
	rm getutsrel
	rm utsrelease.c
	# clean out folder
	rm -rf out
	make clean
	make mrproper
}

post_build() {
	if [ -d $(pwd)/.git ]; then
		GITSHA=$(git rev-parse --short HEAD)
	else
		GITSHA="localbuild"
	fi
	
	AK3="$(pwd)/AnyKernel3"
	DATE=$(date +'%Y%m%d%H%M%S')
	ZIP_FMT="AnyKernel3-`echo $DEVICE`_$GITSHA-$DATE"
	
	clone_ak3;
	if [ -d $AK3 ]; then
		echo "- Creating AnyKernel3"
		gen_getutsrelease;
		if [ -d $(pwd)/out ]; then
			gcc -D__OUT__ -CC utsrelease.c -o getutsrel
		else
			gcc -CC utsrelease.c -o getutsrel
		fi
		UTSRELEASE=$(./getutsrel)
		sed -i "s/kernel\.string=.*/kernel.string=$UTSRELEASE/" "$AK3/anykernel.sh"
		sed -i "s/BLOCK=.*/BLOCK=\/dev\/block\/by-name\/boot;/" "$AK3/anykernel.sh"
		cp $IMAGE $AK3
		cd $AK3
		zip -r9 ../`echo $ZIP_FMT`.zip *
		# CI will clean itself post-build, so we don't need to clean
		# Also avoiding small AnyKernel3 zip issue!
		if [ "$IS_CI" != "true" ] && [ "$DO_CLEAN" = "true" ]; then
			pr_info "Host is not Automated CI, cleaning dirs"
			post_build_clean;
		fi
		cd ..
		pr_err "Build done. Thanks for using this build script :)"
	fi
}

handle_lto() {
	if [[ "$LTO" = "thin" ]]; then
		pr_info "LTO: Thin"
		setconfig disable LTO_NONE
		setconfig enable LTO
		setconfig enable THINLTO
		setconfig enable LTO_CLANG
		setconfig enable ARCH_SUPPORTS_LTO_CLANG
		setconfig enable ARCH_SUPPORTS_THINLTO
	elif [[ "$LTO" = "full" ]]; then
		pr_info "LTO: Full"
		setconfig disable LTO_NONE
		setconfig enable LTO
		setconfig disable THINLTO
		setconfig enable LTO_CLANG
		setconfig enable ARCH_SUPPORTS_LTO_CLANG
		setconfig enable ARCH_SUPPORTS_THINLTO
	fi
}
# call summary
pr_sum
if [ "$BUILD" = "kernel" ]; then
	make -j`echo $ALLOC_JOB` -C $(pwd) O=$(pwd)/out `echo $DEFAULT_ARGS` vendor/`echo $BUILD_DEFCONFIG`
	[ "$KERNELSU" = "true" ] && setconfig enable KSU
	[ "$LTO" != "none" ] && handle_lto || pr_info "LTO not set";
	make -j`echo $ALLOC_JOB` -C $(pwd) O=$(pwd)/out `echo $DEFAULT_ARGS`
	if [ -e $IMAGE ]; then
		pr_post_build "completed"
		post_build
	else
		pr_post_build "failed"
	fi
elif [ "$BUILD" = "defconfig" ]; then
	make -j`echo $ALLOC_JOB` -C $(pwd) O=$(pwd)/out `echo $DEFAULT_ARGS` vendor/`echo $BUILD_DEFCONFIG`
fi