package main

import (
	"dash-server/helper"
	"fmt"
	"io"
	"io/fs"
	"log"
	"mime"
	"net/http"
	"strconv"
	"time"
)

const root http.Dir = ".."

func prog_init() *http.Server { // initialzie the server and other parameters
	mime.AddExtensionType(".map", "application/json; charset=utf-8")
	mime.AddExtensionType(".mpd", "application/dash+xml")

	http.HandleFunc("/", http_handler_default)
	http.HandleFunc("/media_src/multiple/video/", http_handler_video)
	http.HandleFunc("/media_src/multiple/audio/", http_handler_audio)
	//http.HandleFunc("/media_src/single/", http_handler_single)

	return &http.Server{ // configuration for server, using DefaultServeMux
		Addr:        "127.0.0.1:http",
		ReadTimeout: 120 * time.Second,
		/*ReadTimeout is the maximum duration for reading the entire request, including the body.
		other parameters are default now.
		*/
	}
}

/*var t int = 0

func http_handler_single(response http.ResponseWriter, request *http.Request) {
	if (t % 2) == 0 {
		t += 1
		http_handler(response, request, "video/mp4")
	} else {
		t += 1
		http_handler(response, request, "audio/mp4")
	}
}*/

func http_handler_video(response http.ResponseWriter, request *http.Request) {
	http_handler(response, request, "video/mp4")
}

func http_handler_audio(response http.ResponseWriter, request *http.Request) {
	http_handler(response, request, "audio/mp4")
}

func http_handler_default(response http.ResponseWriter, request *http.Request) {
	if request.URL.Path == "/" {
		http_handler(response, request, helper.Get_type_by_extension(".html"))
	} else {
		http_handler(response, request, helper.Get_type_by_extension(request.URL.Path))
	}
}

func http_handler(response http.ResponseWriter, request *http.Request, content_type string) {
	var file_info fs.FileInfo
	var file http.File = nil
	var err error
	var buf []byte
	var read_byte_count, write_byte_count int
	bytes_st := 0
	bytes_end := 0 // for range byte request

	//log.Printf("%v\n", request.URL)
	if request.URL.Path == "/" {
		file, err = root.Open("index.html")
	} else {
		file, err = root.Open(request.URL.Path)
	}
	if err != nil {
		goto err_status
	}
	file_info, err = file.Stat()
	if err != nil {
		goto err_status
	}
	bytes_st, bytes_end, err = helper.Get_range(request.Header.Get("range"), int(file_info.Size()))
	if err != nil {
		goto err_status
	}

	response.Header().Set("content-type", content_type)
	response.Header().Set("content-length", strconv.FormatInt(int64(bytes_end-bytes_st+1), 10))

	response.Header().Set("Access-Control-Allow-Origin", "*")
	response.Header().Set("Access-Control-Allow-Methods", "GET,POST,OPTIONS,HEAD")
	response.Header().Set("Access-Control-Allow-Headers", "range,origin,accept-encoding")

	if bytes_st != 0 || bytes_end == int(file_info.Size()) { // byte-range request
		response.Header().Set("content-range", fmt.Sprintf("bytes %d-%d/%d",
			bytes_st, bytes_end, file_info.Size()))
		response.WriteHeader(http.StatusPartialContent)
	}

	buf = make([]byte, bytes_end-bytes_st+1)
	if request.Method == "HEAD" || request.Method == "OPTIONS" { // no body response for head method
		file.Close()
		response.WriteHeader(http.StatusOK)
		return
	}

	file.Seek(int64(bytes_st), io.SeekStart)
	if read_byte_count, err = file.Read(buf); err != nil || read_byte_count != len(buf) {
		goto err_status
	}
	if write_byte_count, err = response.Write(buf); err != nil || write_byte_count != len(buf) {
		if err != nil {
			log.Println(err)
		} else if write_byte_count != len(buf) {
			log.Printf("write_byte : %v, real_need_write : %v", write_byte_count, len(buf))
		}
	}
	file.Close()
	return

err_status:
	if file != nil {
		file.Close()
	}
	log.Println(err)
	http.NotFound(response, request)
}

func main() {
	prog_init().ListenAndServe()
}
