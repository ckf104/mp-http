package helper

import (
	"fmt"
	"io/ioutil"
	"mime"
	"net/http"
	"strings"
)

func Get_type_by_content(path string) string { // get mime type from content of file, not very accurate i think
	var r http.Dir = ".."
	f, e := r.Open(path)
	if e != nil {
		fmt.Println(e)
		return ""
	}
	buf, e := ioutil.ReadAll(f)
	if e != nil {
		fmt.Println(e)
		return ""
	}
	return http.DetectContentType(buf)
}

func Get_type_by_extension(filename string) string { // get mime type from extension name of file, more accurate i think
	extension_name := filename[strings.LastIndex(filename, "."):]
	return mime.TypeByExtension(extension_name)
}

func Get_range(byte_range string, max_size int) (int, int, error) { // extract string like "bytes=123-456" or "bytes=123-"
	start := 0
	end := max_size - 1
	var err error = nil
	if byte_range != "" {
		tmp := strings.Index(byte_range, "-")
		if tmp == len(byte_range)-1 {
			_, err = fmt.Sscanf(byte_range, "bytes=%d-", &start)
		} else if tmp == 6 {
			_, err = fmt.Sscanf(byte_range, "bytes=-%d", &end)
		} else {
			_, err = fmt.Sscanf(byte_range, "bytes=%d-%d", &start, &end)
		}
	}
	return start, end, err
}
