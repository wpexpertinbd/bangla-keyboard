-- Bangla Keyboard — smart installer app (Install / Reinstall / Uninstall)
-- by BiswasHost  https://www.biswashost.com
-- Privileged commands pass every path through `quoted form of`, so a path with
-- quotes/spaces/metacharacters can never inject a shell command.

property pkgName : "Bangla Keyboard.pkg"
property appTitle : "Bangla Keyboard"

on run
	set layoutFile to "/Library/Keyboard Layouts/Bangla Unicode.keylayout"
	set isInstalled to false
	try
		do shell script "test -e " & quoted form of layoutFile
		set isInstalled to true
	end try

	if isInstalled then
		set act to button returned of (display dialog ¬
			"Bangla Keyboard is already installed on this Mac." & return & return & ¬
			"What would you like to do?" ¬
			buttons {"Cancel", "Uninstall", "Reinstall"} default button "Reinstall" ¬
			cancel button "Cancel" with title appTitle with icon note)
		if act is "Reinstall" then
			doInstall()
		else if act is "Uninstall" then
			doUninstall()
		end if
	else
		set act to button returned of (display dialog ¬
			"Bangla Keyboard is not installed yet." & return & return & ¬
			"Install it now?" ¬
			buttons {"Cancel", "Install"} default button "Install" ¬
			cancel button "Cancel" with title appTitle with icon note)
		if act is "Install" then doInstall()
	end if
end run

on doInstall()
	set pkgPath to ""
	try
		set pkgPath to POSIX path of (path to resource pkgName)
	on error
		display dialog "Installer package not found inside the app." buttons {"OK"} with title appTitle
		return
	end try
	try
		do shell script "/usr/sbin/installer -pkg " & quoted form of pkgPath & " -target /" with administrator privileges
		display dialog ¬
			"Installed successfully." & return & return & ¬
			"Next steps:" & return & ¬
			"1) Log out and back in (or restart)." & return & ¬
			"2) System Settings -> Keyboard -> Text Input -> Edit... -> + -> Bangla -> add the layouts." & return & return & ¬
			"(macOS caches keyboard layouts, so the log-out is required.)" ¬
			buttons {"OK"} default button "OK" with title appTitle
	on error errMsg number errNum
		if errNum is not -128 then display dialog "Install was cancelled or failed." & return & return & (errMsg as text) buttons {"OK"} with title appTitle
	end try
end doInstall

on doUninstall()
	set kl to "/Library/Keyboard Layouts/"
	set fdir to "/Library/Fonts/"
	set layoutFiles to {"Bangla Unicode.keylayout", "Bangla Unicode.icns", "Bangla Classic.keylayout", "Bangla Classic.icns"}
	set fontFiles to {"AdorshoLipi_20-07-2007.ttf", "AponaLohit.ttf", "Bangla.ttf", "BenSen.ttf", "BenSenHandwriting.ttf", "Lohit_14-04-2007.ttf", "Mukti_1.99_PR.ttf", "Siyamrupali.ttf", "SolaimanLipi.ttf", "akaashnormal.ttf", "kalpurush.ttf", "mitra.ttf", "muktinarrow.ttf", "sagarnormal.ttf"}
	set cmd to "/bin/rm -f"
	repeat with p in layoutFiles
		set cmd to cmd & " " & quoted form of (kl & (p as text))
	end repeat
	repeat with f in fontFiles
		set cmd to cmd & " " & quoted form of (fdir & (f as text))
	end repeat
	try
		do shell script cmd with administrator privileges
		display dialog ¬
			"Uninstalled." & return & return & ¬
			"The layout files and the bundled fonts were removed. If the layouts still appear in the input menu, remove them in System Settings -> Keyboard -> Text Input -> Edit..., then log out/in." ¬
			buttons {"OK"} default button "OK" with title appTitle
	on error errMsg number errNum
		if errNum is not -128 then display dialog "Uninstall was cancelled or failed." & return & return & (errMsg as text) buttons {"OK"} with title appTitle
	end try
end doUninstall
