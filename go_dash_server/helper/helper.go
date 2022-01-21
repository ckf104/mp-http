package helper

import (
	"fmt"
	"io/ioutil"
	"mime"
	"net/http"
	"strings"
)

func Get_type_by_content(path string) string { // get mime type from content of file, not very accurate i think
	var r http.Dir = "../../"
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

// get mime type from extension name of file, more accurate i think
func Get_type_by_extension(filename string) string {
	extension_name := filename[strings.LastIndex(filename, "."):]
	return mime.TypeByExtension(extension_name)
}

// extract string like "bytes=123-456" or "bytes=123-"
func Get_range(byte_range string, max_size int) (bool, int, int, error) {
	start := 0
	end := max_size - 1
	has_range_header := false
	var err error = nil
	if byte_range != "" {
		has_range_header = true
		tmp := strings.Index(byte_range, "-")
		if tmp == len(byte_range)-1 {
			_, err = fmt.Sscanf(byte_range, "bytes=%d-", &start)
		} else if tmp == 6 {
			_, err = fmt.Sscanf(byte_range, "bytes=-%d", &end)
		} else {
			_, err = fmt.Sscanf(byte_range, "bytes=%d-%d", &start, &end)
		}
	}
	return has_range_header, start, end, err
}
