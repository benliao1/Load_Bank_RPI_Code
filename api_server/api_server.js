const http = require('http')
const port = process.env.PORT || 6001

const server = http.createServer((req,res) => {
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
  // console.log(url.pathname.endsWith('/') ? url.pathname.slice[0,-1] : url.pathname)
  console.log(`path = ${path}`)
  console.log(url.searchParams);
  // console.log(url.searchParams.get("a"))

  switch(path) {
    case '/api/v1/phases/status':
      spawnCmd(res, "/home/ubuntu/load_bank/prog", ["PHASE?"]);
      break
    case '/api/v1/phases':
      var values = url.searchParams.get("values")
      spawnCmd(res, "/home/ubuntu/load_bank/prog", ["PHASE", values]);
      break
    case '/api/v1/switches/status':
        spawnCmd(res, "/home/ubuntu/load_bank/prog", ["SW?"]);
        break;
    case '/api/v1/switches':
      var values = url.searchParams.get("values")
      spawnCmd(res, "/home/ubuntu/load_bank/prog", ["SW", values]);
      break
    case '/api/v1/zcs/status':
      spawnCmd(res, "/home/ubuntu/load_bank/prog", ["ZCS?"]);
      break
    case '/api/v1/zcs/on':
      spawnCmd(res, "/home/ubuntu/load_bank/prog", ["ZCS", "ON"]);
      break
    case '/api/v1/zcs/off':
      spawnCmd(res, "/home/ubuntu/load_bank/prog", ["ZCS", "OFF"]);
      break
    default:
      res.writeHead(404, { 'Content-Type': 'text/plain' })
      res.end('Not Found')
      break
  } })

server.listen(port, () => console.log(`server started on port ${port}; ` +
  'press Ctrl-C to terminate....'))

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
