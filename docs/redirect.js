const redirect = () => {
  if (!("URLPattern" in globalThis)) return

  const pattern = new URLPattern({ pathname: "/en/*" })
  const match = pattern.exec(location)
  if (!match) return

  const restOfPath = match.pathname.groups[0]
  if (!restOfPath) return
  location.pathname = `/${restOfPath}`
}
redirect()
