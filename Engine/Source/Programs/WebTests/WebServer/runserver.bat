@echo off

if exist env (
	call .\env\Scripts\activate.bat
) else (
	..\..\..\..\Binaries\ThirdParty\Python3\Win64\python.exe -m venv env
	call .\env\Scripts\activate.bat
	pip install -r requirements.txt
	if %ERRORLEVEL% NEQ 0 (
		rmdir /s /q env
		exit %ERRORLEVEL%
	)
)

start "HTTP Server" python manage.py runserver 0.0.0.0:8000
@REM start "HTTPS Server" python manage.py runsslserver 0.0.0.0:8001 --certificate ssl/https_server_certificate.crt --key ssl/https_server.key
