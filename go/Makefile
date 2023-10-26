
BIN="bin/goJpgStream"

build: clean
	@echo 'building...'
	@env CGO_ENABLED=0 GOOS=windows GOARCH=amd64 go build -ldflags '-s -w' -gcflags="all=-trimpath=${PWD}" -asmflags="all=-trimpath=${PWD}" -o ${BIN}_win_x64.exe main.go
	@env CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags '-s -w' -gcflags="all=-trimpath=${PWD}" -asmflags="all=-trimpath=${PWD}" -o ${BIN}_linux_x64 main.go
	@env CGO_ENABLED=0 GOOS=linux GOARCH=arm GOARM=7 go build -ldflags '-s -w' -gcflags="all=-trimpath=${PWD}" -asmflags="all=-trimpath=${PWD}" -o ${BIN}_arm_v7 main.go
	@env CGO_ENABLED=0 GOOS=darwin GOARCH=arm64 go build -ldflags '-s -w' -gcflags="all=-trimpath=${PWD}" -asmflags="all=-trimpath=${PWD}" -o ${BIN}_darwin_arm64 main.go
	@env CGO_ENABLED=0 GOOS=darwin GOARCH=amd64 go build -ldflags '-s -w' -gcflags="all=-trimpath=${PWD}" -asmflags="all=-trimpath=${PWD}" -o ${BIN}_darwin_x64 main.go

clean:
	@rm -rf bin/*

zip: clean build
	@zip -qrmo ${BIN}_win_x64.zip ${BIN}_win_x64.exe
	@zip -qrmo ${BIN}_linux_x64.zip ${BIN}_linux_x64
	@zip -qrmo ${BIN}_arm_v7.zip ${BIN}_arm_v7
	@zip -qrmo ${BIN}_darwin_arm64.zip ${BIN}_darwin_arm64
	@zip -qrmo ${BIN}_darwin_x64.zip ${BIN}_darwin_x64