"""
File to build custom topology for appendix example
"""

from mininet.cli import CLI
from mininet.log import setLogLevel
from mininet.net import Mininet
from mininet.topo import Topo
from mininet.node import RemoteController, OVSSwitch

class CliqueTopo( Topo ):
	"A clique topology with 4 switches with 2 hosts each"

	def build( self ):
		h1s1 = self.addHost("h1s1")
		h2s1 = self.addHost("h2s1")
		h1s2 = self.addHost("h1s2")
		h2s2 = self.addHost("h2s2")
		h1s3 = self.addHost("h1s3")
		h2s3 = self.addHost("h2s3")
		h1s4 = self.addHost("h1s4")
		h2s4 = self.addHost("h2s4")

		s1 = self.addSwitch("s1")
		s2 = self.addSwitch("s2")
		s3 = self.addSwitch("s3")
		s4 = self.addSwitch("s4")

		self.addLink(s1,h1s1)
		self.addLink(s1,h2s1)
		self.addLink(s2,h1s2)
		self.addLink(s2,h2s2)
		self.addLink(s3,h1s3)
		self.addLink(s3,h2s3)
		self.addLink(s4,h1s4)
		self.addLink(s4,h2s4)

		self.addLink(s1,s2)
		self.addLink(s1,s3)
		self.addLink(s1,s4)
		self.addLink(s2,s3)
		self.addLink(s2,s4)
		self.addLink(s3,s4)

class SharedLinksTopo( Topo ):
	"A topology with 2 switches that each have 2 hosts and 3 links between them"

	def build( self ):
		h1s1 = self.addHost("h1s1")
		h2s1 = self.addHost("h2s1")
		h1s2 = self.addHost("h1s2")
		h2s2 = self.addHost("h2s2")

		s1 = self.addSwitch("s1")
		s2 = self.addSwitch("s2")

		self.addLink(s1,h1s1)
		self.addLink(s1,h2s1)
		self.addLink(s2,h1s2)
		self.addLink(s2,h2s2)

		self.addLink(s1,s2)
		self.addLink(s1,s2)
		self.addLink(s1,s2)

topos = {
	'clique': CliqueTopo,
	'shared-links': SharedLinksTopo
}
