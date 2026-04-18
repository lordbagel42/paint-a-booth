import { betterAuth } from 'better-auth';
import { drizzleAdapter } from 'better-auth/adapters/drizzle';
import { sveltekitCookies } from 'better-auth/svelte-kit';
import { genericOAuth } from 'better-auth/plugins';
import { env } from '$env/dynamic/private';
import { getRequestEvent } from '$app/server';
import { getDb } from '$lib/server/db';

export function getAuth() {
	const event = getRequestEvent();
	const db = getDb(event!.platform!.env.DB);
	return betterAuth({
		baseURL: env.ORIGIN,
		secret: env.BETTER_AUTH_SECRET,
		database: drizzleAdapter(db, { provider: 'sqlite' }),
		plugins: [
			genericOAuth({
				config: [
					{
						providerId: 'hackclub',
						discoveryUrl: 'https://auth.hackclub.com/.well-known/openid-configuration',
						clientId: env.HACKCLUB_CLIENT_ID!,
						clientSecret: env.HACKCLUB_CLIENT_SECRET!,
						scopes: ['openid', 'email', 'name']
					}
				]
			}),
			sveltekitCookies(getRequestEvent) // must be last
		]
	});
}
