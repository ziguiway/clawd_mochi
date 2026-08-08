const offlineButton = document.getElementById('offline-button');
const onboardingMessage = document.getElementById('onboarding-msg');

offlineButton?.addEventListener('click', async () => {
    offlineButton.disabled = true;
    onboardingMessage.className = 'status-msg info';
    onboardingMessage.textContent = 'Saving offline mode...';
    try {
        const response = await fetch('/onboarding/offline', {method: 'POST'});
        if (!response.ok) throw new Error('request failed');
        onboardingMessage.className = 'status-msg success';
        onboardingMessage.textContent = 'Offline mode is ready. Opening the controller...';
        setTimeout(() => { window.location.href = '/'; }, 700);
    } catch (error) {
        offlineButton.disabled = false;
        onboardingMessage.className = 'status-msg error';
        onboardingMessage.textContent = 'Could not save this choice. Try again.';
    }
});
