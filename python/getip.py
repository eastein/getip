from twisted.web import server, resource
from twisted.internet import reactor

class GetIP(resource.Resource) :
	isLeaf = True
	def render_GET(self, request) :
		request.setHeader("Content-Type", "text/plain")
		fw = request.getHeader("X-Forwarded-For")
		if fw :
			ip = fw.split(',')[0]
		else :
			ip = request.getClientIP()

		return str(ip + "\r\n")

site = server.Site(GetIP())
reactor.listenTCP(8080, site)
reactor.run()
