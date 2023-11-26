// define http object
const http = require('http')

// define port the api server will run on
const port = process.env.PORT || 6001

// define the absolute path of the serial interface program on the raspberry pi
const serialInterfacePath = "/home/ubuntu/load_bank/serial_interface/serial_interface"

// Define the API server object
const server = http.createServer( (req,res) => {

	console.log(`req.headers.host = ${req.headers.host}`)
	console.log(`request.url = ${req.url}`)
	const url = new URL(req.url, `http://${req.headers.host}`)
	console.log(`url = ${url}`)
	var path =  url.pathname
	if (url.pathname.endsWith('/')) {
		console.log(`trailing slash = ${url.pathname}`)
		console.log(`removing trailing slash = ${url.pathname.substr(0, url.pathname.length -1)}`)
		path =  url.pathname.substr(0, url.pathname.length -1)
	}

	console.log(`path = ${path}`)
	console.log(url.searchParams);

	// depending on the URL supplied, call the serial interface with the correct arguments
	switch(path) {
		case '/api/v1/phases/status':
			spawnCmd(res, serialInterfacePath, ["PHASE?"]);
			break
		case '/api/v1/phases':
			var values = url.searchParams.get("values")
			spawnCmd(res, serialInterfacePath, ["PHASE", values]);
			break
		case '/api/v1/switches/status':
			spawnCmd(res, serialInterfacePath, ["SW?"]);
			break;
		case '/api/v1/switches':
			var values = url.searchParams.get("values")
			spawnCmd(res, serialInterfacePath, ["SW", values]);
			break
		case '/api/v1/zcs/status':
			spawnCmd(res, serialInterfacePath, ["ZCS?"]);
			break
		case '/api/v1/zcs/on':
			spawnCmd(res, serialInterfacePath, ["ZCS", "ON"]);
			break
		case '/api/v1/zcs/off':
			spawnCmd(res, serialInterfacePath, ["ZCS", "OFF"]);
			break
		default:
			res.writeHead(404, { 'Content-Type': 'text/plain' })
			res.end('Not Found')
			break
		}
	})

// start the API server
server.listen(port, () => console.log(`server started on port ${port}; ` +
  'press Ctrl-C to terminate....'))

// run a command in the command line from Javascript and put the resuld in res
function spawnCmd(res, cmd, args) {
	const { spawn } = require("child_process")
	if (args === "") {
		var ls = spawn(cmd)
	} else {
		var ls = spawn(cmd, args)
	}

	console.log(`command is ${cmd} ${args}`)

	ls.stdout.on("data", data => {
		console.log(`stdout: ${data}`)
		res.writeHead(200, { 'Content-Type': 'text/plain' })
		res.end(`${data}`)
	});

	ls.stderr.on("data", data => {
		console.log(`stderr: ${data}`)
		res.writeHead(500, { 'Content-Type': 'text/plain' })
		res.end(`${data}`)
	});

	ls.on('error', (error) => {
		console.log(`error: ${error.message}`)
		res.writeHead(500, { 'Content-Type': 'text/plain' })
		res.end(`${error.message}`)
	});

	ls.on("close", code => {
		console.log(`child process exited with code ${code}`)
	});
}
