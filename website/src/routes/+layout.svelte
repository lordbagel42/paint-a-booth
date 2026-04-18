<script lang="ts">
	import './layout.css';
	import favicon from '$lib/assets/favicon.svg';
	import { authClient } from '$lib/auth-client';
	import { invalidateAll } from '$app/navigation';

	let { data, children } = $props();

	async function login() {
		await authClient.signIn.social({ provider: 'hackclub', callbackURL: '/' });
	}

	async function logout() {
		await authClient.signOut({ fetchOptions: { onSuccess: () => invalidateAll() } });
	}
</script>

<svelte:head><link rel="icon" href={favicon} /></svelte:head>

<div class="flex min-h-screen flex-col">
	<header class="flex items-center justify-between px-4 py-2 text-sm">
		{#if data.user}
			<span class="font-medium">{data.user.name}</span>
			<button class="text-neutral-500 hover:underline" onclick={logout}>Logout</button>
		{:else}
			<span></span>
			<button class="rounded bg-blue-600 px-3 py-1 text-white hover:bg-blue-700" onclick={login}
				>Login with Hack Club</button
			>
		{/if}
	</header>
	{@render children()}
</div>
