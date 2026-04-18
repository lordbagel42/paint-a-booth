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
						scopes: ['openid', 'profile'],
						async getUserInfo(tokens) {
							const res = await fetch('https://auth.hackclub.com/oauth/userinfo', {
								headers: {
									Authorization: `Bearer ${tokens.accessToken}`
								}
							});
							if (!res.ok) return null;
							const data = await res.json();
							return {
								id: data.sub,
								name: data.name || data.preferred_username || 'Unknown',
								email: data.email || `${data.sub}@hackclub.user`,
								emailVerified: !!data.email_verified,
								image: data.picture
							};
						}
					}
				]
			}),
			sveltekitCookies(getRequestEvent) // must be last
		]
	});
}
