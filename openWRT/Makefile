# This is the makefile for the openwrt build system.
#
# copy this file to:
#		trunk/package/squeezed
#
# copy the files in the parent directory to:
#		trunk/package/squeezed/files
#
# You can compile this package alone by running:
#		cd trunk
#		make package/squeezed-compile


include $(TOPDIR)/rules.mk
PKG_NAME:=squeezed
PKG_RELEASE:=1
PKG_VERSION:=0.1
#PKG_SOURCE:=squeezed-$(PKG_VERSION).tgz
#PKG_SOURCE_URL:=@SF/squeezed
PKG_BUILD_DEPENDS:=+taglib +libpthread

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)


include $(INCLUDE_DIR)/package.mk

define Package/squeezed
	SECTION:=multimedia
	CATEGORY:=Multimedia
	TITLE:=Server for logitech squeezebox
	DEPENDS:=+taglib +libpthread
endef


define Package/squeezed/description
	Provides a server for logitech's squeezebox, see www.slimdevices.com
	It supports only mp3 files at the moment, searching is limited.
endef


CONFIGURE_VARS:= \
	CC="$(TARGET_CC)" \
	LD="$(TARGET_LD)" \
	CFLAGS="$(TARGET_CFLAGS) -I$(STAGING_DIR)/usr/lib/libtag/include -I$(STAGING_DIR)/usr/include" \
	LDFLAGS="$(TARGET_LDFLAGS) -L$(STAGING_DIR)/usr/lib/libtag/lib -lstdc++ -lpthread" \
	LIBTOOL="$(STAGING_DIR)/host/bin/libtool" \
	STRIP=$(STRIP) \
	HOST="openWRT"


define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./files/* $(PKG_BUILD_DIR) 
endef


define Build/Configure
#	( cd $(PKG_BUILD_DIR); tar -xzf squeezed.tgz; \
#	$(CONFIGURE_VARS) ./configure $(CONFIGURE_ARGS); \

endef


define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(CONFIGURE_VARS)
endef


define Package/squeezed/install
		$(INSTALL_DIR) $(1)/usr/bin
		$(INSTALL_BIN) $(PKG_BUILD_DIR)/bin/openWRT/squeezed $(1)/usr/bin/
#		$(INSTALL_DIR) $(1)/usr/lib
endef

$(eval $(call BuildPackage,squeezed))
