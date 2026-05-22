self.addEventListener('notificationclick', event => {
  event.notification.close();

  event.waitUntil((async () => {
    const windows = await clients.matchAll({ type: 'window', includeUncontrolled: true });
    const openWindow = windows.find(client => client.url.includes(self.location.origin));

    if (openWindow) {
      await openWindow.focus();
      return;
    }

    await clients.openWindow('/');
  })());
});
