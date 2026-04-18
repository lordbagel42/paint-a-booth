<script lang="ts">
	import './layout.css';
	import favicon from '$lib/assets/favicon.svg';

	let { data, children } = $props();
</script>

<svelte:head><link rel="icon" href={favicon} /></svelte:head>

<div class="flex min-h-screen flex-col">
	<header class="flex items-center justify-between px-4 py-2 text-sm">
		{#if data.user}
			<span class="font-medium">{data.user.name}</span>
			<a href="/api/auth/sign-out" rel="external" class="text-neutral-500 hover:underline">Logout</a
			>
		{:else}
			<span></span>
			<a
				href="/api/auth/sign-in/social?provider=hackclub"
				rel="external"
				class="rounded bg-blue-600 px-3 py-1 text-white hover:bg-blue-700">Login with Hack Club</a
			>
		{/if}
	</header>
	{@render children()}
</div>
