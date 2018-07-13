#encoding=utf-8

# Pigeon Client for Testing

import sys
import urllib.request, urllib.parse
import html
import os
import subprocess
import threading
import time
import datetime
import hashlib
import base64
import json

#





def md5sum(s):
	return hashlib.md5(s.encode("utf-8")).hexdigest()

def md5sum_b(s):
	return hashlib.md5(s).hexdigest()

def read_file(name, fallback = ""):
	try:
		f = open(name, "r")
		res = f.read()
		f.close()
		return res
	except:
		return fallback

def read_file_b(name, fallback = b""):
	try:
		f = open(name, "rb")
		res = f.read()
		f.close()
		return res
	except:
		return fallback


def do_post(url, data_dict):
	try:
		data = urllib.parse.urlencode(data_dict).encode()
		req = urllib.request.Request(url, data=data)
		res = urllib.request.urlopen(req)
		return res.read().decode('utf-8')
	except:
		return ""



def main():
	argc = len(sys.argv) - 1
	if (argc != 4) and (argc != 2):
		print("Usage: submit.py <pigeon-url> <task-id> <contestant-zip> <problem-zip>")
		print("Usage: submit.py <pigeon-url> <task-id>  # query")
		return
	url = sys.argv[1]
	if url[-1:] != "/":
		url += "/"
	task_id = sys.argv[2]
	if argc == 4:
		contestant_zip = sys.argv[3]
		problem_zip = sys.argv[4]
		print("zip files %s %s" % (contestant_zip, problem_zip))
		
		contestant_content = read_file_b(contestant_zip)
		contestant_md5 = md5sum_b(contestant_content)
		print("contestant zip %s %s" % (len(contestant_content), contestant_md5))
		res = do_post(url + "api/send_file", {"md5": contestant_md5, "content": base64.b64encode(contestant_content)})
		print(res)
		
		problem_content = read_file_b(problem_zip)
		problem_md5 = md5sum_b(problem_content)
		res = do_post(url + "api/send_file", {"md5": problem_md5, "content": base64.b64encode(problem_content)})
		print(res)
		
		res = do_post(url + "api/submit_task", {
			"taskid": task_id,
			"contestant_md5": contestant_md5,
			"problem_md5": problem_md5,
		})
		print(res)
	else:
		res = do_post(url + "api/get_task_results", {"taskids": task_id})
		print(json.dumps(json.loads(res), indent=4, sort_keys=True))
#

main()
