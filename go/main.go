package main

import (
	"bytes"
	"container/list"
	"log"
	"net"
	"strconv"
	"sync"
	"time"

	"github.com/gin-gonic/gin"
)

var mCams map[string]*Cams

type Cams struct {
	Num     int
	Buf     *list.List
	FPS     int
	Mu      sync.RWMutex
	LED     []byte
	JpgExit chan struct{}
	UdpExit chan struct{}
}

func main() {
	mCams = make(map[string]*Cams)

	gin.SetMode(gin.ReleaseMode)
	app := gin.Default()

	app.GET("/stream/:id/:fps", StreamHandle)
	app.GET("/led/:id/:state", LedHandle)
	if err := app.Run(":8080"); err != nil {
		log.Panicln(err)
	}
}

func LedHandle(c *gin.Context) {
	id := c.Param("id")
	state := c.Param("state")
	if _, ok := mCams[id]; ok {
		if state == "" || state == "0" {
			mCams[id].LED = []byte{0x00}
		} else if state == "1" {
			mCams[id].LED = []byte{0x01}
		}
	}
	c.Status(200)
}

func StreamHandle(c *gin.Context) {
	c.Header("Content-Type", "multipart/x-mixed-replace; boundary=frame")
	boundary := "\r\n--frame\r\nContent-Type: image/jpeg\r\n\r\n"
	id := c.Param("id")
	address := net.ParseIP(id)
	if address == nil {
		log.Println("ip addr err.")
		return
	}
	fps, _ := strconv.Atoi(c.Param("fps"))
	if fps == 0 {
		fps = 10
	}
	if fps > 25 {
		fps = 25
	}

	if _, ok := mCams[id]; !ok {
		go udpInit(id)
		mCams[id] = &Cams{Num: 0, Buf: list.New(), FPS: 0, LED: []byte{0x00}, JpgExit: make(chan struct{}, 1), UdpExit: make(chan struct{}, 1)}
	}
	mCams[id].FPS = fps
	mCams[id].Num++
	log.Println(id, "+client num", mCams[id].Num)
	for {
		select {
		case <-mCams[id].JpgExit:
			return
		default:
			val := mCams[id].Buf
			if val == nil {
				continue
			}
			if val.Front() == nil {
				continue
			}
			buf := val.Front().Value.([]byte)
			c.Writer.WriteString(boundary)
			_, err := c.Writer.Write(buf)

			if err != nil {
				if mCams[id].Num == 1 {
					mCams[id].UdpExit <- struct{}{}
				} else {
					mCams[id].Num--
					log.Println(id, "-client num", mCams[id].Num)
				}
				return
			}
			time.Sleep(time.Millisecond * time.Duration(1000/mCams[id].FPS))
		}
	}
}

func udpInit(id string) {
	retry := 0
	for {
		conn, err := net.Dial("udp", id+":9122")
		if err != nil {
			log.Println(err)
			time.Sleep(time.Second)
			continue
		}
		buf := []byte{}
		b := make([]byte, 1500)
		for {
			select {
			case <-mCams[id].UdpExit:
				conn.Close()
				log.Println("conn", id, "clean.")
				delete(mCams, id)
				return
			default:
				if retry == 100 {
					mCams[id].JpgExit <- struct{}{}
					mCams[id].UdpExit <- struct{}{}
				}
				conn.SetReadDeadline(time.Now().Add(time.Millisecond * 200))
				conn.Write(mCams[id].LED)
				n, err := conn.Read(b)
				if err != nil {
					log.Println(err)
					time.Sleep(time.Millisecond * 10)
					retry++
					continue
				}
				retry = 0
				if n == 4 && bytes.HasSuffix(b[:n], []byte{0x23, 0x23, 0x23}) {
					a8 := add8(buf)
					if a8 == b[n-4] {
						if mCams[id].Buf.Len() > 1 {
							mCams[id].Buf.Remove(mCams[id].Buf.Front())
						}
						mCams[id].Buf.PushBack(buf)
					}
					buf = []byte{}
					time.Sleep(time.Millisecond * 20)
					conn.Write(mCams[id].LED)
					continue
				}
				buf = append(buf, b[:n]...)
			}
		}
	}
}

func add8(buf []byte) byte {
	b := byte(0)
	for i := 0; i < len(buf); i++ {
		b += buf[i]
	}
	return b & 0xFF
}
