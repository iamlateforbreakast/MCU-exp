Issues boot Fedora 43 live USB image

Potential Causes and Solutions
USB Boot is Disabled in BIOS: Many Dell Latitude models ship with external USB ports disabled for booting to prevent unauthorised OS installations.
Fix: Restart and tap F2 to enter BIOS. Navigate to System Configuration > USB Configuration and ensure Enable External USB Port and Enable USB Boot Support are checked.

Secure Boot Interference: While Fedora 43 supports Secure Boot, sometimes a specific hardware/firmware combination on a second-hand machine might block "untrusted" media.
Fix: In the BIOS, go to Secure Boot and set it to Disabled temporarily to see if the drive appears.

Fastboot Setting: If "Fastboot" is set to "Minimal" in the BIOS, it may skip initializing USB ports during the boot process to save time.
Fix: In the BIOS, go to POST Behavior > Fastboot and select Thorough.

Storage Controller Mode (RAID vs. AHCI): Dells often ship in RAID mode, which can cause issues with Linux installers recognizing the drive even if the USB boots.
Fix: In the BIOS, under Storage, change the SATA/NVMe Operation from RAID to AHCI/NVMe.

USB Formatting (UEFI/GPT): If the USB works on an older computer but not this one, it might be formatted with an MBR partition table for Legacy BIOS. The Latitude 7340 requires a GPT partition table for UEFI.
Fix: Re-create your live media using the Fedora Media Writer or Rufus (selecting GPT and UEFI target system). 
