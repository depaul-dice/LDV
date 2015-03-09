class Publication:
	counter = 1
	DATE, AUTHORS, INFO, KW = [1,2,3,4]

	def __init__(self, *args, **kwargs):
		pid = "id_" + str(Publication.counter)
		Publication.counter += 1
		date, authors, info, keywords = args
		self.id = kwargs.get("id", pid)
		self.date = date
		self.authors = authors
		self.info = info
		self.keywords = keywords

	def __repr__(self):
		return str([self.id, self.date, self.authors, self.info, self.keywords])

	def jsonInfo(self):
		return [self.id, self.date, self.authors, self.info]

	def jsonKeyword(self):
		return [self.id, self.date, self.keywords]