include $(TOPDIR)/rules.mk

PKG_NAME:=risc1run
PKG_RELEASE:=1
CMAKE_INSTALL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/cmake.mk

define Package/risc1run
	SECTION:=utils
	CATEGORY:=Utilities
	DEPENDS:=@TARGET_elvees_mcom03 +kmod-elvees-risc1 +libyaml
	TITLE:=Simple risc1 control utility
endef

define Package/risc1run/description
   risc1run is a simple utility to start RISC1 program
endef

TARGET_CFLAGS += -I$(STAGING_DIR_ROOT)/usr/include
TARGET_LDFLAGS += -lyaml

define Package/risc1run/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/risc1run $(1)/usr/bin/
endef

$(eval $(call BuildPackage,risc1run))

