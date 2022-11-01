# elv-risc1run

Утилита для RISC1 собирается в OpenWRT.
Для сборки требуется драйвер [elrisc1](https://github.com/dis-projects/elrisc1)

Предполагается, что утилита будет расположена в директории
elv-openwrt/package/utils/risc1run

Для добавления утилиты следует использовать make menuconfig
```
Utilities  --->
    <*> risc1run.................................... Simple risc1 control utility
```